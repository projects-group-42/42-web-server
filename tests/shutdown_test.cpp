/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   shutdown_test.cpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: galves-a <galves-a@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/08/14 17:20:00 by galves-a          #+#    #+#             */
/*   Updated: 2026/08/14 17:20:00 by galves-a         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <ctime>
#include <dirent.h>
#include <poll.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>

#include "server/EventLoop.hpp"
#include "config/ConfigLoader.hpp"
#include "config/ServerConfig.hpp"

static int	s_pass = 0;
static int	s_fail = 0;

#define TEST(cond, name) \
	do { \
		if (cond) { s_pass++; std::cout << "[PASS] " << name << std::endl; } \
		else { s_fail++; std::cerr << "[FAIL] " << name << std::endl; } \
	} while (0)

/*
 * Each test binds a port of its own, so the one before it can still be sitting
 * in TIME_WAIT without turning into a bind failure here.
 */
static const int	PORT_DRAIN = 18077;
static const int	PORT_SILENT = 18078;
static const int	PORT_CGI = 18079;
static const int	PORT_FDS = 18080;

/*
 * The response the drain test asks for. It has to be larger than what the
 * socket buffers on both ends can swallow in one write, or the loop would hand
 * the whole body to the kernel before the stop ever arrives and the test would
 * pass without a drain existing at all.
 */
static const size_t	BIG_BODY_SIZE = 8 * 1024 * 1024;
static const char	*BIG_BODY_PATH = "www/shutdown_big.bin";
static const char	*CONFIG_PATH = "shutdown_test.conf";

/**
 * @brief Counts the descriptors this process holds open.
 * A shutdown that forgot a socket or a CGI pipe leaves the process holding
 * more than it started with, which is what the count catches. /dev/fd is read
 * when it is there, and a probe for the lowest free descriptor stands in for
 * it when it is not: either is enough to compare a before against an after.
 * @return The number of open descriptors, or the lowest free one as a proxy.
 */
static int	openFdCount(void)
{
	DIR	*dir = opendir("/dev/fd");

	if (dir != NULL)
	{
		int				count = 0;
		struct dirent	*entry;

		while ((entry = readdir(dir)) != NULL)
		{
			if (entry->d_name[0] != '.')
				++count;
		}
		closedir(dir);
		return (count - 1);
	}

	int	probe = dup(0);

	if (probe == -1)
		return (-1);
	close(probe);
	return (probe);
}

/**
 * @brief Writes a config naming a single server block on a test port.
 * @param port The port the block listens on.
 */
static void	writeConfig(int port)
{
	std::ofstream	file(CONFIG_PATH);

	file << "server {\n"
		<< "    listen 127.0.0.1:" << port << ";\n"
		<< "    server_name localhost;\n"
		<< "    client_max_body_size 1M;\n"
		<< "    location / {\n"
		<< "        root www/;\n"
		<< "        index index.html;\n"
		<< "        limit_except GET;\n"
		<< "    }\n"
		<< "}\n";
	file.close();
}

/**
 * @brief Starts a server in a child process, bound to the given config.
 * @return The pid of the child running the loop.
 */
static pid_t	forkServer(void)
{
	pid_t	server = fork();

	if (server != 0)
		return (server);

	ConfigLoader				loader(CONFIG_PATH);
	std::vector<ServerConfig>	configs = loader.loader();
	EventLoop					loop(configs);

	EventLoop::setupSignals();
	loop.setupSockets();
	loop.run();
	_exit(0);
}

/**
 * @brief Opens a client connection to a test port.
 * @param port The port to reach.
 * @return The connected descriptor, or -1 when the server is not reachable.
 */
static int	connectToServer(int port)
{
	struct sockaddr_in	address;
	int					fd = socket(AF_INET, SOCK_STREAM, 0);

	if (fd == -1)
		return (-1);
	address.sin_family = AF_INET;
	address.sin_port = htons(port);
	address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	if (connect(fd, (struct sockaddr *)&address, sizeof(address)) == -1)
	{
		close(fd);
		return (-1);
	}
	return (fd);
}

/**
 * @brief Connects to a test port, waiting for the server to come up.
 * A fixed sleep is the wrong tool for "the child has bound by now": it is
 * either longer than it needs to be or, on a loaded machine, not long enough,
 * and the second way it fails looks exactly like the bug the test is hunting.
 * @param port The port to reach.
 * @return The connected descriptor, or -1 when it never came up.
 */
static int	connectWhenReady(int port)
{
	for (int attempt = 0; attempt < 100; ++attempt)
	{
		int	fd = connectToServer(port);

		if (fd != -1)
			return (fd);
		usleep(50000);
	}
	return (-1);
}

/**
 * @brief Reads from fd until the peer closes or the window runs out.
 * @param fd The connection to drain.
 * @param timeoutMs How long to wait for each chunk.
 * @return Everything that arrived.
 */
static std::string	readAll(int fd, int timeoutMs)
{
	std::string	out;

	while (true)
	{
		struct pollfd	pfd;
		char			buffer[65536];
		ssize_t			bytes;

		pfd.fd = fd;
		pfd.events = POLLIN;
		pfd.revents = 0;
		if (poll(&pfd, 1, timeoutMs) <= 0)
			return (out);
		bytes = recv(fd, buffer, sizeof(buffer), 0);
		if (bytes <= 0)
			return (out);
		out.append(buffer, static_cast<size_t>(bytes));
	}
}

/*
 * The point of the drain: a response the server had already serialised reaches
 * its client whole, even though the stop was asked for while it was still
 * being written.
 *
 * The client asks for a body far larger than the socket buffers and then reads
 * nothing, so the loop fills the kernel and stalls with megabytes still in the
 * connection's write buffer. The stop arrives at exactly that point, and what
 * decides the test is whether the body that finally comes back is the whole
 * one: without a drain the connection is closed where it stood and the client
 * is left holding a truncated response.
 */
static void	test_pending_response_survives_the_stop(void)
{
	std::ofstream	body(BIG_BODY_PATH);

	body << std::string(BIG_BODY_SIZE, 'x');
	body.close();
	writeConfig(PORT_DRAIN);

	pid_t	server = forkServer();
	int		client = connectWhenReady(PORT_DRAIN);

	if (client == -1)
	{
		TEST(false, "client reaches the server");
		kill(server, SIGKILL);
		waitpid(server, NULL, 0);
		std::remove(CONFIG_PATH);
		std::remove(BIG_BODY_PATH);
		return ;
	}

	std::string	request =
		"GET /shutdown_big.bin HTTP/1.1\r\nHost: localhost\r\n\r\n";

	send(client, request.data(), request.size(), 0);
	usleep(500000);
	kill(server, SIGINT);

	std::string	response = readAll(client, 4000);
	int			status = 0;

	waitpid(server, &status, 0);

	size_t	headerEnd = response.find("\r\n\r\n");
	size_t	bodyBytes = (headerEnd == std::string::npos)
					? 0 : response.size() - headerEnd - 4;

	TEST(response.find("HTTP/1.1 200") != std::string::npos,
		"a response already queued is still answered after the stop");
	TEST(bodyBytes == BIG_BODY_SIZE,
		"the drain delivers the whole body instead of a truncated one");
	TEST(WIFEXITED(status) && WEXITSTATUS(status) == 0,
		"the server exits cleanly rather than being killed");
	close(client);
	std::remove(CONFIG_PATH);
	std::remove(BIG_BODY_PATH);
}

/*
 * The other half of the same promise: the stop stays bounded. A client that
 * connects and never says a word holds a connection the server has nothing to
 * finish for, and the drain must not wait on it — only the connections that
 * actually hold bytes are polled, so a silent one costs nothing.
 *
 * Three seconds is the ceiling enforced against a 2s window: room for the
 * second of resolution nowMs() carries, and far short of the forever a drain
 * that polled every connection would take.
 */
static void	test_a_silent_client_does_not_hold_the_stop(void)
{
	writeConfig(PORT_SILENT);

	pid_t	server = forkServer();
	int		silent = connectWhenReady(PORT_SILENT);

	TEST(silent != -1, "a silent client is connected");
	usleep(200000);

	time_t	start = std::time(NULL);

	kill(server, SIGINT);
	waitpid(server, NULL, 0);

	double	elapsed = std::difftime(std::time(NULL), start);

	TEST(elapsed <= 3.0,
		"a connection with nothing pending does not stretch the shutdown");
	if (silent != -1)
		close(silent);
	std::remove(CONFIG_PATH);
}

/*
 * A CGI child has no response to lose and is killed where it stands, but it
 * must not be left behind: an unreaped child outlives the server as an orphan,
 * and a server that leaves processes running has not released its resources
 * whatever its descriptors say.
 *
 * The script sleeps far past the CGI deadline, so what ends it can only be the
 * shutdown, and it is looked for by name once the server is gone.
 */
static void	test_cgi_child_does_not_outlive_the_server(void)
{
	std::ofstream	config(CONFIG_PATH);

	config << "server {\n"
		<< "    listen 127.0.0.1:" << PORT_CGI << ";\n"
		<< "    server_name localhost;\n"
		<< "    location /cgi {\n"
		<< "        root cgi-bin;\n"
		<< "        cgi_pass .sh /bin/sh;\n"
		<< "    }\n"
		<< "}\n";
	config.close();

	std::ofstream	script("cgi-bin/shutdown_sleep.sh");

	script << "sleep 60\n";
	script.close();

	pid_t	server = forkServer();
	int		client = connectWhenReady(PORT_CGI);

	if (client == -1)
	{
		TEST(false, "client reaches the server for the CGI request");
		kill(server, SIGKILL);
		waitpid(server, NULL, 0);
		std::remove(CONFIG_PATH);
		std::remove("cgi-bin/shutdown_sleep.sh");
		return ;
	}

	std::string	request =
		"GET /cgi/shutdown_sleep.sh HTTP/1.1\r\nHost: localhost\r\n\r\n";

	send(client, request.data(), request.size(), 0);
	usleep(500000);
	kill(server, SIGINT);
	waitpid(server, NULL, 0);
	usleep(300000);

	int	found = std::system("pgrep -f shutdown_sleep.sh > /dev/null 2>&1");

	TEST(found != 0, "no CGI child outlives the server it was started by");
	close(client);
	std::remove(CONFIG_PATH);
	std::remove("cgi-bin/shutdown_sleep.sh");
}

/*
 * The acceptance criterion, measured rather than asserted: the loop is built,
 * bound and served in this process, and the descriptors it holds once it has
 * stopped are compared with the ones it held before it started. A listening
 * socket, an accepted client or a CGI pipe the shutdown forgot is a count that
 * came back higher.
 *
 * A helper child does the asking, so what the test drives is the path a real
 * SIGINT takes rather than a private entry point: it connects, sends a request
 * the loop answers, and only then signals its parent. The connection is still
 * open at that point, the client having asked for keep-alive, so the count
 * covers the accepted socket and not just the listener.
 *
 * This one runs last because it cannot be undone: the handler lowers a flag
 * that lives for the rest of the process, and a server forked after it would
 * stop before it ever served anything.
 */
static void	test_shutdown_leaves_no_descriptors_behind(void)
{
	writeConfig(PORT_FDS);
	EventLoop::setupSignals();

	int		before = openFdCount();
	pid_t	helper = fork();

	if (helper == 0)
	{
		pid_t	parent = getppid();
		int		fd = connectWhenReady(PORT_FDS);

		if (fd != -1)
		{
			std::string	request = "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n";

			send(fd, request.data(), request.size(), 0);
			usleep(400000);
		}
		kill(parent, SIGINT);
		usleep(200000);
		if (fd != -1)
			close(fd);
		_exit(0);
	}

	{
		ConfigLoader				loader(CONFIG_PATH);
		std::vector<ServerConfig>	configs = loader.loader();
		EventLoop					loop(configs);

		loop.setupSockets();
		loop.run();
	}
	waitpid(helper, NULL, 0);

	int	after = openFdCount();

	TEST(before == after,
		"a stopped server holds no descriptor it did not start with");
	std::remove(CONFIG_PATH);
}

int	main(void)
{
	signal(SIGPIPE, SIG_IGN);
	test_pending_response_survives_the_stop();
	test_a_silent_client_does_not_hold_the_stop();
	test_cgi_child_does_not_outlive_the_server();
	test_shutdown_leaves_no_descriptors_behind();
	std::cout << std::endl << s_pass << " passed, " << s_fail
		<< " failed" << std::endl;
	return (s_fail == 0 ? 0 : 1);
}

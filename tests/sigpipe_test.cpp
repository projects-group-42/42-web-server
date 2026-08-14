/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   sigpipe_test.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: galves-a <galves-a@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/08/10 00:00:00 by galves-a          #+#    #+#             */
/*   Updated: 2026/08/10 00:00:00 by galves-a         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstdio>
#include <csignal>
#include <poll.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>

#include "server/EventLoop.hpp"
#include "cgi/CgiProcess.hpp"

static int	s_pass = 0;
static int	s_fail = 0;

#define TEST(cond, name) \
	do { \
		if (cond) { s_pass++; std::cout << "[PASS] " << name << std::endl; } \
		else { s_fail++; std::cerr << "[FAIL] " << name << std::endl; } \
	} while (0)

/**
 * @brief Opens a connected pair of stream sockets, one end standing for the
 * server and the other for the client.
 * @param serverSide Receives the end the writes under test are issued on.
 * @param clientSide Receives the end that is closed to stand for a client
 * going away.
 * @return true when the pair was created.
 */
static bool	makeSocketPair(int &serverSide, int &clientSide)
{
	int	pair[2];

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == -1)
		return (false);
	serverSide = pair[0];
	clientSide = pair[1];
	return (true);
}

/**
 * @brief Opens a real TCP connection over the loopback interface.
 * The listening socket is bound to port 0 so the kernel picks a free port and
 * the test never collides with a port already in use.
 * @param serverSide Receives the accepted end, which stands for the connection
 * the server holds.
 * @param clientSide Receives the connected end, which stands for the client.
 * @return true when both ends were established.
 */
static bool	makeLoopbackPair(int &serverSide, int &clientSide)
{
	struct sockaddr_in	address;
	socklen_t			length = sizeof(address);
	int					listener = socket(AF_INET, SOCK_STREAM, 0);

	if (listener == -1)
		return (false);
	address.sin_family = AF_INET;
	address.sin_port = 0;
	address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	if (bind(listener, (struct sockaddr *)&address, sizeof(address)) == -1
		|| listen(listener, 1) == -1
		|| getsockname(listener, (struct sockaddr *)&address, &length) == -1)
	{
		close(listener);
		return (false);
	}
	clientSide = socket(AF_INET, SOCK_STREAM, 0);
	if (clientSide == -1
		|| connect(clientSide, (struct sockaddr *)&address, length) == -1)
	{
		close(listener);
		return (false);
	}
	serverSide = accept(listener, NULL, NULL);
	close(listener);
	return (serverSide != -1);
}

/**
 * @brief Closes a connected socket the way a client that vanishes does.
 * A zero linger makes the close send a reset instead of a graceful shutdown,
 * which is what a client killed mid-response leaves behind.
 * @param fd The end to close abruptly.
 */
static void	closeAbruptly(int fd)
{
	struct linger	nolinger;

	nolinger.l_onoff = 1;
	nolinger.l_linger = 0;
	setsockopt(fd, SOL_SOCKET, SO_LINGER, &nolinger, sizeof(nolinger));
	close(fd);
}

/**
 * @brief Writes content into path so a CGI run can be pointed at a real file.
 * @param path The file to write.
 * @param content The script body.
 */
static void	writeScript(const std::string &path, const std::string &content)
{
	std::ofstream	file(path.c_str());

	file << content;
	file.close();
}

/**
 * @brief Drains a CGI process to completion the way the event loop does.
 * @param proc The process to drive.
 */
static void	driveToCompletion(CgiProcess &proc)
{
	while (proc.isReading())
	{
		struct pollfd	pfd;

		pfd.fd = proc.outputReadFd();
		pfd.events = POLLIN;
		pfd.revents = 0;
		if (poll(&pfd, 1, 2000) <= 0)
			break ;
		proc.onReadable();
	}
}

/*
 * The premise of the whole issue: a write to a socket whose peer is gone is
 * answered with SIGPIPE, and a process that leaves that signal at its default
 * is killed by it. The check runs in a child so the death can be observed, and
 * it is what makes the tests below mean something: if this one ever stops
 * failing that way, the server is no longer being protected from anything.
 */
static void	test_default_disposition_kills_the_process(void)
{
	pid_t	child = fork();

	if (child == 0)
	{
		int	serverSide;
		int	clientSide;

		signal(SIGPIPE, SIG_DFL);
		if (!makeSocketPair(serverSide, clientSide))
			_exit(3);
		close(clientSide);
		send(serverSide, "x", 1, 0);
		_exit(0);
	}

	int	status = 0;

	waitpid(child, &status, 0);
	TEST(WIFSIGNALED(status) && WTERMSIG(status) == SIGPIPE,
		"writing to a gone peer kills a process that leaves SIGPIPE alone");
}

/*
 * The server installs its dispositions before anything else, so the same write
 * comes back as -1 and the process is still there to read it. handleSend turns
 * that -1 into one dropped client.
 */
static void	test_server_survives_write_to_closed_peer(void)
{
	int	serverSide;
	int	clientSide;

	if (!makeSocketPair(serverSide, clientSide))
	{
		TEST(false, "socket pair created");
		return ;
	}
	close(clientSide);

	ssize_t	sent = send(serverSide, "x", 1, 0);

	TEST(sent == -1, "send to a gone peer reports failure instead of killing us");
	TEST(true, "the process is still running after that write");
	close(serverSide);
}

/*
 * The acceptance criterion itself, over a real TCP connection: a client that
 * resets the connection mid-response is written to several times, as a response
 * larger than the socket buffer would be, and the server is still standing
 * afterwards. The first write after a reset fails on its own; the ones that
 * follow are the writes that raise SIGPIPE, so the sending goes on past the
 * first failure instead of stopping at it.
 */
static void	test_server_survives_client_reset(void)
{
	int	serverSide;
	int	clientSide;

	if (!makeLoopbackPair(serverSide, clientSide))
	{
		TEST(false, "loopback connection established");
		return ;
	}
	closeAbruptly(clientSide);

	std::string	chunk(64 * 1024, 'x');
	int			failures = 0;

	for (int i = 0; i < 200; ++i)
	{
		if (send(serverSide, chunk.data(), chunk.size(), 0) <= 0)
			++failures;
	}
	TEST(failures > 0, "writing to a reset connection reports failure");
	TEST(true, "the server outlives a client that hangs up abruptly");
	close(serverSide);
}

/*
 * An ignored signal stays ignored through execve, so a CGI child would run the
 * script under the server's disposition unless it is put back. The script sends
 * itself a SIGPIPE: under the default it dies before printing anything, and
 * printing is exactly what it does when the server's ignore leaked into it.
 */
static void	test_cgi_child_runs_with_default_sigpipe(void)
{
	std::vector<std::string>	env;
	CgiProcess					proc(-1, "");

	writeScript("cgi_sigpipe.sh", "kill -s PIPE $$\necho leaked\n");
	TEST(proc.start("/bin/sh", "cgi_sigpipe.sh", env),
		"CGI process starts");
	driveToCompletion(proc);
	TEST(proc.output().empty(),
		"a CGI child is killed by SIGPIPE rather than inheriting the ignore");
	TEST(proc.reap() == -1, "a child killed by a signal reports no clean exit");
	std::remove("cgi_sigpipe.sh");
}

int	main(void)
{
	EventLoop::setupSignals();
	test_default_disposition_kills_the_process();
	test_server_survives_write_to_closed_peer();
	test_server_survives_client_reset();
	test_cgi_child_runs_with_default_sigpipe();
	std::cout << std::endl << s_pass << " passed, " << s_fail
		<< " failed" << std::endl;
	return (s_fail == 0 ? 0 : 1);
}

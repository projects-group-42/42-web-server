/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   timeout_test.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: galves-a <galves-a@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/08/10 00:00:00 by galves-a          #+#    #+#             */
/*   Updated: 2026/08/10 00:00:00 by galves-a         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <iostream>
#include <string>
#include <vector>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "server/EventLoop.hpp"
#include "network/Connection.hpp"
#include "config/ServerConfig.hpp"

static int	s_pass = 0;
static int	s_fail = 0;

# define TEST(cond, name) \
	do { \
		if (cond) { s_pass++; std::cout << "[PASS] " << name << std::endl; } \
		else { s_fail++; std::cerr << "[FAIL] " << name << std::endl; } \
	} while (0)

static const int	TIMEOUT_TEST_PORT = 18642;
static const double	TEST_IDLE_TIMEOUT = 2.0;
static const int	READ_WINDOW_MS = 15000;

/**
 * @brief Builds the single server block the forked server listens on.
 * @return The server blocks, one root'd at www so a complete request is served.
 */
static std::vector<ServerConfig>	makeConfigs(void)
{
	std::vector<ServerConfig>	configs;
	ServerConfig				config;

	config.host = "127.0.0.1";
	config.port = TIMEOUT_TEST_PORT;
	config.root = "www";
	configs.push_back(config);
	return (configs);
}

/**
 * @brief Forks a server running with a timeout short enough to wait on.
 * The child reports through a pipe that its socket is bound before entering
 * the loop, so the parent never races the first connect, and its logging is
 * sent to /dev/null so it does not interleave with the results. Returns -1
 * when the child died before binding, which is what a port already taken looks
 * like from here.
 * @param pid Set to the pid of the server so the caller can reap it.
 * @return True when the server is listening.
 */
static bool	startServer(pid_t &pid)
{
	int		ready[2];
	char	token = 0;

	if (pipe(ready) == -1)
		return (false);
	pid = fork();
	if (pid == -1)
	{
		close(ready[0]);
		close(ready[1]);
		return (false);
	}
	if (pid == 0)
	{
		close(ready[0]);
		if (!freopen("/dev/null", "w", stdout) || !freopen("/dev/null", "w", stderr))
			_exit(1);
		try
		{
			EventLoop	loop(makeConfigs());

			loop.setIdleTimeout(TEST_IDLE_TIMEOUT);
			loop.setupSockets();
			if (write(ready[1], "1", 1) != 1)
				_exit(1);
			close(ready[1]);
			loop.run();
		}
		catch (const std::exception &)
		{
			_exit(1);
		}
		_exit(0);
	}
	close(ready[1]);
	if (read(ready[0], &token, 1) != 1)
	{
		close(ready[0]);
		waitpid(pid, NULL, 0);
		return (false);
	}
	close(ready[0]);
	return (true);
}

/**
 * @brief Opens a connection to the forked server.
 * @return The connected socket, or -1 when the connection could not be made.
 */
static int	connectToServer(void)
{
	int					fd = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in	addr;

	if (fd == -1)
		return (-1);
	std::memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(TIMEOUT_TEST_PORT);
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
	{
		close(fd);
		return (-1);
	}
	return (fd);
}

/**
 * @brief Reads from fd until the peer closes or the window runs out.
 * @param fd The socket to drain.
 * @param out Receives every byte read.
 * @return True when the peer closed the connection, false on a timeout, which
 * is what a connection the server failed to reap looks like from here.
 */
static bool	readUntilClose(int fd, std::string &out)
{
	long	deadline = static_cast<long>(std::time(NULL)) * 1000 + READ_WINDOW_MS;

	out.clear();
	while (true)
	{
		struct pollfd	pfd;
		long			remaining = deadline
							- static_cast<long>(std::time(NULL)) * 1000;
		int				ready;

		if (remaining <= 0)
			return (false);
		pfd.fd = fd;
		pfd.events = POLLIN;
		pfd.revents = 0;
		ready = poll(&pfd, 1, static_cast<int>(remaining));
		if (ready == -1)
			return (false);
		if (ready == 0)
			return (false);
		char	buffer[4096];
		ssize_t	n = recv(fd, buffer, sizeof(buffer), 0);

		if (n == 0)
			return (true);
		if (n < 0)
			return (false);
		out.append(buffer, static_cast<size_t>(n));
	}
}

/**
 * @brief Reads whatever arrives within ms milliseconds without waiting for a
 * close, so a response can be read off a connection meant to stay open.
 * @param fd The socket to read from.
 * @param ms How long to wait for the first bytes.
 * @param out Receives every byte read.
 * @return True when at least one byte arrived.
 */
static bool	readAvailable(int fd, int ms, std::string &out)
{
	struct pollfd	pfd;
	char			buffer[4096];
	ssize_t			n;

	out.clear();
	pfd.fd = fd;
	pfd.events = POLLIN;
	pfd.revents = 0;
	if (poll(&pfd, 1, ms) <= 0)
		return (false);
	n = recv(fd, buffer, sizeof(buffer), 0);
	if (n <= 0)
		return (false);
	out.append(buffer, static_cast<size_t>(n));
	return (true);
}

/**
 * @brief Feeds a connection built over a socketpair the given bytes.
 * @param peer The end of the pair standing in for the client.
 * @param conn The connection under test.
 * @param data The bytes the client sends.
 * @return True when the bytes were written and read back into the parser.
 */
static bool	feed(int peer, Connection &conn, const std::string &data)
{
	if (write(peer, data.data(), data.size())
			!= static_cast<ssize_t>(data.size()))
		return (false);
	return (conn.receive_data() > 0);
}

/*
 * Checks what the connection reports about a request in flight, which is what
 * the sweep reads to tell a client that stalled halfway through a request from
 * one that has simply gone quiet.
 */
static void	runConnectionTests(void)
{
	int	pair[2];

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == -1)
	{
		std::cerr << "[FAIL] socketpair() failed" << std::endl;
		s_fail++;
		return ;
	}

	Connection	conn(pair[1]);

	TEST(conn.has_partial_request() == false,
		"a connection that has said nothing has no request in flight");
	TEST(conn.timed_out() == false, "a fresh connection is not timed out");
	TEST(conn.is_idle(1000.0) == false,
		"a fresh connection is not idle against a long timeout");
	TEST(conn.is_idle(0.0) == true,
		"every connection is idle against a timeout of zero");

	TEST(feed(pair[0], conn, "GET / HT"),
		"a split request line reaches the parser");
	TEST(conn.has_partial_request() == true,
		"a half-read request line counts as a request in flight");

	TEST(feed(pair[0], conn, "TP/1.1\r\nHost: localhost\r\n"),
		"the rest of the request line and a header reach the parser");
	TEST(conn.has_partial_request() == true,
		"headers still to come count as a request in flight");

	TEST(feed(pair[0], conn, "\r\n"), "the blank line reaches the parser");
	TEST(conn.get_psr_state() == COMPLETE, "the request is complete");
	TEST(conn.has_partial_request() == false,
		"a complete request is not a request in flight");

	conn.mark_timed_out();
	TEST(conn.timed_out() == true, "a connection answered with 408 is marked");
	conn.reset_for_next_request();
	TEST(conn.timed_out() == false, "reuse clears the timed out mark");
	TEST(conn.has_partial_request() == false,
		"a connection waiting between requests has no request in flight");

	close(pair[0]);
}

/*
 * Checks the guard on the timeout setter, since a timeout of zero or less
 * would close a connection on the same turn of the loop that accepted it.
 */
static void	runTimeoutSettingTests(void)
{
	EventLoop	loop(makeConfigs());
	double		shipped = loop.getIdleTimeout();

	TEST(shipped > 0.0, "the server ships with a timeout of its own");
	loop.setIdleTimeout(0.0);
	TEST(loop.getIdleTimeout() == shipped, "a timeout of zero is refused");
	loop.setIdleTimeout(-5.0);
	TEST(loop.getIdleTimeout() == shipped, "a negative timeout is refused");
	loop.setIdleTimeout(TEST_IDLE_TIMEOUT);
	TEST(loop.getIdleTimeout() == TEST_IDLE_TIMEOUT,
		"a positive timeout is accepted");
}

/*
 * Drives a live server the way a client would, since what the issue asks for
 * is that a descriptor stops being held: every case here ends with the server
 * closing the connection on its own, and a test that hangs instead of reading
 * end-of-file is the failure being guarded against.
 */
static void	runServerTests(void)
{
	std::string	response;
	int			fd;

	fd = connectToServer();
	TEST(fd != -1, "a client can connect to the server");
	if (fd != -1)
	{
		const std::string	partial = "GET / HTTP/1.1\r\nHost: localhost\r\n";

		TEST(send(fd, partial.data(), partial.size(), 0)
			== static_cast<ssize_t>(partial.size()),
			"a client can send half a request");
		TEST(readUntilClose(fd, response),
			"a client that stalls mid-request is closed by the server");
		TEST(response.find("408") != std::string::npos,
			"a client that stalls mid-request is answered 408");
		close(fd);
	}

	fd = connectToServer();
	if (fd != -1)
	{
		TEST(readUntilClose(fd, response),
			"a client that never speaks is closed by the server");
		TEST(response.empty(),
			"a client that never speaks is closed without a response");
		close(fd);
	}

	fd = connectToServer();
	if (fd != -1)
	{
		const std::string	request = "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n";

		send(fd, request.data(), request.size(), 0);
		TEST(readAvailable(fd, 5000, response) && response.find("HTTP/1.1")
			!= std::string::npos, "a complete request is answered");
		TEST(response.find("408") == std::string::npos,
			"a complete request is not answered 408");
		TEST(readUntilClose(fd, response),
			"a kept-alive connection left idle is closed by the server");
		close(fd);
	}

	fd = connectToServer();
	if (fd != -1)
	{
		const std::string	head = "GET / HTTP/1.1\r\n";
		const std::string	tail = "Host: localhost\r\n\r\n";
		int					i = 0;
		bool				sent = true;

		while (i < 4)
		{
			struct timespec	pause;

			pause.tv_sec = 1;
			pause.tv_nsec = 0;
			nanosleep(&pause, NULL);
			if (i == 0)
				sent = send(fd, head.data(), head.size(), 0)
					== static_cast<ssize_t>(head.size());
			else
				sent = sent && send(fd, "X-Wait: 1\r\n", 11, 0) == 11;
			++i;
		}
		TEST(sent, "a client trickling a request keeps the socket writable");
		send(fd, tail.data(), tail.size(), 0);
		TEST(readAvailable(fd, 5000, response)
			&& response.find("408") == std::string::npos,
			"a client that keeps sending is not timed out");
		close(fd);
	}
}

int	main(void)
{
	pid_t	pid = -1;

	signal(SIGPIPE, SIG_IGN);
	runConnectionTests();
	runTimeoutSettingTests();
	if (!startServer(pid))
	{
		std::cerr << "[FAIL] could not start the server on port "
			<< TIMEOUT_TEST_PORT << std::endl;
		s_fail++;
	}
	else
	{
		runServerTests();
		kill(pid, SIGKILL);
		waitpid(pid, NULL, 0);
	}
	std::cout << "passed: " << s_pass << ", failed: " << s_fail << std::endl;
	return (s_fail == 0 ? 0 : 1);
}

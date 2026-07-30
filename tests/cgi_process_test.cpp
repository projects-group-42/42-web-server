/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   cgi_process_test.cpp                               :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: galves-a <galves-a@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/24 00:00:00 by galves-a          #+#    #+#             */
/*   Updated: 2026/07/24 00:00:00 by galves-a         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstdio>
#include <poll.h>

#include "cgi/CgiProcess.hpp"

static int	s_pass = 0;
static int	s_fail = 0;

#define TEST(cond, name) \
	do { \
		if (cond) { s_pass++; std::cout << "[PASS] " << name << std::endl; } \
		else { s_fail++; std::cerr << "[FAIL] " << name << std::endl; } \
	} while (0)

/*
 * Writes content into path so the test can point a CGI run at a real file.
 */
static void	writeScript(const std::string &path, const std::string &content)
{
	std::ofstream	file(path.c_str());

	file << content;
	file.close();
}

/*
 * Drives the process to completion the way the event loop does: polls the
 * active pipe fds and calls a single incremental step per ready fd, never
 * blocking on either direction.
 */
static void	driveToCompletion(CgiProcess &proc)
{
	while (!proc.finished())
	{
		struct pollfd	fds[2];
		nfds_t			count = 0;
		int				outIndex = -1;
		int				bodyIndex = -1;

		if (proc.isReading())
		{
			fds[count].fd = proc.outputReadFd();
			fds[count].events = POLLIN;
			fds[count].revents = 0;
			outIndex = static_cast<int>(count);
			++count;
		}
		if (proc.isWriting())
		{
			fds[count].fd = proc.bodyWriteFd();
			fds[count].events = POLLOUT;
			fds[count].revents = 0;
			bodyIndex = static_cast<int>(count);
			++count;
		}
		if (count == 0 || poll(fds, count, 2000) <= 0)
			break;
		if (proc.isWriting() && (fds[bodyIndex].revents & (POLLOUT | POLLERR | POLLHUP)))
		{
			if (fds[bodyIndex].revents & (POLLERR | POLLHUP))
				proc.stopWriting();
			else
				proc.onWritable();
		}
		if (proc.isReading() && (fds[outIndex].revents & (POLLIN | POLLHUP | POLLERR)))
			proc.onReadable();
	}
}

/*
 * A script that only writes to stdout must be drained incrementally into the
 * output accumulator and exit cleanly.
 */
static void	test_incremental_output(void)
{
	std::vector<std::string>	env;
	CgiProcess					proc(-1, "");

	writeScript("cgi_p_echo.sh", "echo hello-proc\n");
	TEST(proc.start("/bin/sh", "cgi_p_echo.sh", env), "start returns true");
	driveToCompletion(proc);
	TEST(proc.finished(), "process reaches finished state");
	TEST(proc.output() == "hello-proc\n", "accumulates script stdout");
	TEST(proc.reap() == 0, "reap returns clean exit status");
	std::remove("cgi_p_echo.sh");
}

/*
 * A script that echoes stdin proves the body is streamed to the child while
 * its output is drained at the same time.
 */
static void	test_body_forwarded(void)
{
	std::vector<std::string>	env;
	CgiProcess					proc(-1, "ping");

	writeScript("cgi_p_cat.sh", "cat\n");
	TEST(proc.start("/bin/sh", "cgi_p_cat.sh", env), "start returns true with a body");
	TEST(proc.isWriting(), "writing direction is active with a body");
	driveToCompletion(proc);
	TEST(proc.output() == "ping", "forwards request body to script stdin");
	std::remove("cgi_p_cat.sh");
}

/*
 * A body larger than the pipe buffer must not deadlock: the incremental
 * writing and reading interleave through poll until the whole body round-trips.
 */
static void	test_large_body(void)
{
	std::vector<std::string>	env;
	std::string					body(1024 * 1024, 'x');
	CgiProcess					proc(-1, body);

	writeScript("cgi_p_cat.sh", "cat\n");
	TEST(proc.start("/bin/sh", "cgi_p_cat.sh", env), "start returns true on a large body");
	driveToCompletion(proc);
	TEST(proc.output() == body, "streams a body larger than the pipe buffer");
	std::remove("cgi_p_cat.sh");
}

/*
 * An empty body must close the child's stdin immediately so a script that
 * reads stdin still terminates.
 */
static void	test_empty_body_closes_stdin(void)
{
	std::vector<std::string>	env;
	CgiProcess					proc(-1, "");

	writeScript("cgi_p_cat.sh", "cat\n");
	TEST(proc.start("/bin/sh", "cgi_p_cat.sh", env), "start returns true with empty body");
	TEST(!proc.isWriting(), "writing direction is inactive with empty body");
	driveToCompletion(proc);
	TEST(proc.finished(), "process finishes with empty body");
	TEST(proc.output().empty(), "no output from an empty stdin");
	std::remove("cgi_p_cat.sh");
}

int	main(void)
{
	test_incremental_output();
	test_body_forwarded();
	test_large_body();
	test_empty_body_closes_stdin();
	std::cout << std::endl << s_pass << " passed, " << s_fail
		<< " failed" << std::endl;
	return (s_fail == 0 ? 0 : 1);
}

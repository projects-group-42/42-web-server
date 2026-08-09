/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   cgi_test_runner.hpp                                :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: galves-a <galves-a@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/08/09 00:00:00 by galves-a          #+#    #+#             */
/*   Updated: 2026/08/09 00:00:00 by galves-a         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CGI_TEST_RUNNER_HPP
# define CGI_TEST_RUNNER_HPP

# include "cgi/CgiProcess.hpp"
# include <poll.h>
# include <errno.h>
# include <string>
# include <vector>

static const int	CGI_TEST_POLL_TIMEOUT_MS = 5000;

/*
 * Drives the process to completion the way the event loop does: polls the
 * active pipe fds and calls a single incremental step per ready fd, never
 * blocking on either direction. Returns false when poll fails or the child
 * stops making progress before both directions are done.
 */
static bool	driveToCompletion(CgiProcess &proc)
{
	while (!proc.finished())
	{
		struct pollfd	fds[2];
		nfds_t			count = 0;
		int				outIndex = -1;
		int				bodyIndex = -1;
		int				ready;

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
		if (count == 0)
			return (false);
		ready = poll(fds, count, CGI_TEST_POLL_TIMEOUT_MS);
		if (ready == -1 && errno == EINTR)
			continue;
		if (ready <= 0)
			return (false);
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
	return (true);
}

/*
 * Runs a CGI script to completion over the non-blocking CgiProcess API and
 * collects its stdout into output. Reaps the child before reporting the
 * result, so a run that stalled still leaves no zombie behind. Returns false
 * on fork or pipe failure, when the poll-driven pump did not finish, or when
 * the script did not exit cleanly with status 0.
 */
static bool	runCgi(const std::string &interpreter, const std::string &scriptPath,
		const std::string &body, const std::vector<std::string> &env,
		std::string &output)
{
	CgiProcess	proc(-1, body);
	bool		driven;
	int			status;

	if (!proc.start(interpreter, scriptPath, env))
		return (false);
	driven = driveToCompletion(proc);
	status = proc.reap();
	output = proc.output();
	return (driven && status == 0);
}

#endif

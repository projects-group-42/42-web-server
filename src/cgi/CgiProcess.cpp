/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   CgiProcess.cpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: galves-a <galves-a@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/24 00:00:00 by galves-a          #+#    #+#             */
/*   Updated: 2026/07/24 00:00:00 by galves-a         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "cgi/CgiProcess.hpp"
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>

/*
 * Puts fd into non-blocking mode so a single read or write can never stall the
 * main loop. Returns false when the flags cannot be read or updated.
 */
static bool setNonBlocking(int fd)
{
	int	flags = fcntl(fd, F_GETFL, 0);

	if (flags == -1)
		return (false);
	return (fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1);
}

/*
 * Runs in the child after fork: redirects the pipe ends onto stdin/stdout,
 * closes the leftover pipe fds, then execve's the interpreter with the script
 * as argv[1] and the prepared CGI environment. Never returns; _exit is called
 * if any step fails.
 */
static void runChild(CgiPipes &pipes, const std::string &interpreter, const std::string &scriptPath, char **envp)
{
	char	*argv[3];

	pipes.closeParentEnds();
	if (dup2(pipes.bodyReadFd(), STDIN_FILENO) == -1)
		_exit(1);
	if (dup2(pipes.outputWriteFd(), STDOUT_FILENO) == -1)
		_exit(1);
	pipes.closeChildEnds();
	argv[0] = const_cast<char *>(interpreter.c_str());
	argv[1] = const_cast<char *>(scriptPath.c_str());
	argv[2] = NULL;
	execve(interpreter.c_str(), argv, envp);
	_exit(1);
}

CgiProcess::CgiProcess(int clientFd, const std::string &body)
	: _pid(-1), _clientFd(clientFd), _body(body), _sent(0),
	  _writing(false), _reading(false), _reaped(true)
{
}

CgiProcess::~CgiProcess(void)
{
	terminate();
}

/*
 * Creates the pipes and forks the CGI child. In the parent it closes the child
 * pipe ends, sets the parent ends non-blocking, and marks which directions are
 * still active: reading is always on, writing only when there is a body (an
 * empty body closes the child's stdin at once). Returns false on pipe or fork
 * failure.
 */
bool CgiProcess::start(const std::string &interpreter, const std::string &scriptPath, const std::vector<std::string> &env)
{
	std::vector<char *>	envp;

	for (size_t i = 0; i < env.size(); ++i)
		envp.push_back(const_cast<char *>(env[i].c_str()));
	envp.push_back(NULL);
	if (!_pipes.create())
		return (false);
	_pid = fork();
	if (_pid == -1)
		return (false);
	if (_pid == 0)
		runChild(_pipes, interpreter, scriptPath, &envp[0]);
	_reaped = false;
	_pipes.closeChildEnds();
	setNonBlocking(_pipes.outputReadFd());
	_reading = true;
	if (_body.empty())
		_pipes.closeBodyWrite();
	else
	{
		setNonBlocking(_pipes.bodyWriteFd());
		_writing = true;
	}
	return (true);
}

/*
 * Reads one ready chunk of the child's output into the accumulator. On EOF or
 * on a hard read error it closes the output pipe and stops reading; EAGAIN is
 * ignored so the direction stays active for the next poll.
 */
void CgiProcess::onReadable(void)
{
	char	buffer[4096];
	ssize_t	bytes = read(_pipes.outputReadFd(), buffer, sizeof(buffer));

	if (bytes > 0)
		_output.append(buffer, static_cast<size_t>(bytes));
	else if (bytes == 0)
	{
		_pipes.closeOutputRead();
		_reading = false;
	}
	else if (errno != EAGAIN && errno != EWOULDBLOCK)
	{
		_pipes.closeOutputRead();
		_reading = false;
	}
}

/*
 * Writes one ready chunk of the request body into the child's stdin. When the
 * whole body has been sent, or on a hard write error, it closes the body pipe
 * and stops writing; EAGAIN is ignored so the direction stays active.
 */
void CgiProcess::onWritable(void)
{
	ssize_t	written = write(_pipes.bodyWriteFd(), _body.data() + _sent, _body.size() - _sent);

	if (written == -1)
	{
		if (errno != EAGAIN && errno != EWOULDBLOCK)
			stopWriting();
		return ;
	}
	_sent += static_cast<size_t>(written);
	if (_sent == _body.size())
		stopWriting();
}

/*
 * Closes the child's stdin and stops the writing direction. Called when the
 * body is fully sent or when the child has closed its read end.
 */
void CgiProcess::stopWriting(void)
{
	_pipes.closeBodyWrite();
	_writing = false;
}

/*
 * Reaps the child once its output is drained and returns its exit status, or
 * -1 when it was already reaped, waitpid failed, or it did not exit normally.
 */
int CgiProcess::reap(void)
{
	int	status;

	if (_reaped)
		return (-1);
	if (waitpid(_pid, &status, 0) == -1)
	{
		_reaped = true;
		return (-1);
	}
	_reaped = true;
	if (WIFEXITED(status))
		return (WEXITSTATUS(status));
	return (-1);
}

/*
 * Kills and reaps the child if it is still running. Used on cleanup paths such
 * as a client disconnecting mid-execution, and by the destructor.
 */
void CgiProcess::terminate(void)
{
	if (!_reaped && _pid > 0)
	{
		::kill(_pid, SIGKILL);
		waitpid(_pid, NULL, 0);
		_reaped = true;
	}
}

bool CgiProcess::isReading(void) const
{
	return (_reading);
}

bool CgiProcess::isWriting(void) const
{
	return (_writing);
}

bool CgiProcess::finished(void) const
{
	return (!_reading && !_writing);
}

int CgiProcess::clientFd(void) const
{
	return (_clientFd);
}

int CgiProcess::outputReadFd(void) const
{
	return (_pipes.outputReadFd());
}

int CgiProcess::bodyWriteFd(void) const
{
	return (_pipes.bodyWriteFd());
}

const std::string &CgiProcess::output(void) const
{
	return (_output);
}

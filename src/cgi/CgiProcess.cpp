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
#include <signal.h>
#include <cstring>
#include <sys/wait.h>

/*
 * Puts fd into non-blocking mode so a single read or write can never stall the
 * main loop. The current flags are not read back first because the subject
 * only authorises fcntl() with F_SETFL, O_NONBLOCK and FD_CLOEXEC, and these
 * pipes are created here with no other flag to preserve. Returns false when
 * the mode cannot be set.
 */
static bool setNonBlocking(int fd)
{
	return (fcntl(fd, F_SETFL, O_NONBLOCK) != -1);
}

/*
 * Rewrites, in place, any SCRIPT_FILENAME or PATH_TRANSLATED entry of envp to
 * `script`. After the child moves into the script's directory those variables
 * must name the script relative to the new working directory, or an
 * interpreter that locates the script through the environment rather than
 * argv (php-cgi reads SCRIPT_FILENAME) looks for it under the wrong path and
 * answers "No input file specified". The replacement strings are owned by the
 * caller so they outlive the execve.
 */
static void rebaseScriptEnv(char **envp, const std::string &script,
		std::string &scriptFilename, std::string &pathTranslated)
{
	for (int i = 0; envp[i] != NULL; ++i)
	{
		if (std::strncmp(envp[i], "SCRIPT_FILENAME=", 16) == 0)
		{
			scriptFilename = "SCRIPT_FILENAME=" + script;
			envp[i] = const_cast<char *>(scriptFilename.c_str());
		}
		else if (std::strncmp(envp[i], "PATH_TRANSLATED=", 16) == 0)
		{
			pathTranslated = "PATH_TRANSLATED=" + script;
			envp[i] = const_cast<char *>(pathTranslated.c_str());
		}
	}
}

/*
 * Runs in the child after fork: redirects the pipe ends onto stdin/stdout,
 * closes the leftover pipe fds, moves into the directory holding the script so
 * it can reach its own files by relative path, then execve's the interpreter
 * with the script name as argv[1] and the prepared CGI environment. Never
 * returns; _exit is called if any step fails.
 */
static void runChild(CgiPipes &pipes, const std::string &interpreter, const std::string &scriptPath, char **envp)
{
	char					*argv[3];
	std::string				script = scriptPath;
	std::string				scriptFilename;
	std::string				pathTranslated;
	std::string::size_type	slash = scriptPath.find_last_of('/');

	pipes.closeParentEnds();
	if (dup2(pipes.bodyReadFd(), STDIN_FILENO) == -1)
		_exit(1);
	if (dup2(pipes.outputWriteFd(), STDOUT_FILENO) == -1)
		_exit(1);
	pipes.closeChildEnds();
	if (slash != std::string::npos)
	{
		if (chdir(scriptPath.substr(0, slash).c_str()) == -1)
			_exit(1);
		script = scriptPath.substr(slash + 1);
		rebaseScriptEnv(envp, script, scriptFilename, pathTranslated);
	}
	argv[0] = const_cast<char *>(interpreter.c_str());
	argv[1] = const_cast<char *>(script.c_str());
	argv[2] = NULL;
	execve(interpreter.c_str(), argv, envp);
	_exit(1);
}

CgiProcess::CgiProcess(int clientFd, const std::string &body)
	: _pid(-1), _clientFd(clientFd), _body(body), _sent(0),
	  _writing(false), _reading(false), _reaped(true), _deadlineMs(0)
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
 * Reads one ready chunk of the child's output into the accumulator. The return
 * value of read() alone decides: only a positive count carries data, while 0
 * (the child closed its stdout) and -1 (an error on a pipe poll() had reported
 * as readable) both close the output pipe and end the reading direction.
 */
void CgiProcess::onReadable(void)
{
	char	buffer[4096];
	ssize_t	bytes = read(_pipes.outputReadFd(), buffer, sizeof(buffer));

	if (bytes > 0)
	{
		_output.append(buffer, static_cast<size_t>(bytes));
		return ;
	}
	_pipes.closeOutputRead();
	_reading = false;
}

/*
 * Writes one ready chunk of the request body into the child's stdin. The return
 * value of write() alone decides: a write that moved no byte (0) or failed (-1)
 * on a pipe poll() had reported as writable closes the body pipe, as does the
 * body being fully sent.
 */
void CgiProcess::onWritable(void)
{
	ssize_t	written = write(_pipes.bodyWriteFd(), _body.data() + _sent, _body.size() - _sent);

	if (written <= 0)
	{
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

/*
 * Stores the absolute time in milliseconds after which the child is considered
 * stuck and must be killed. Set by the event loop when the CGI starts.
 */
void CgiProcess::setDeadlineMs(long deadlineMs)
{
	_deadlineMs = deadlineMs;
}

/*
 * Returns the absolute deadline in milliseconds set by setDeadlineMs.
 */
long CgiProcess::deadlineMs(void) const
{
	return (_deadlineMs);
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

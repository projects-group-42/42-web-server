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
#include <limits.h>
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
 * Caps how much stdout a single CGI run may accumulate before it is treated as
 * misbehaving and killed. Without a cap, a script that keeps producing output
 * forever would keep pushing its own inactivity deadline back (every chunk
 * read counts as progress) while _output grows without bound, eventually
 * failing an allocation and taking the whole server down with it. 10MB is
 * generous for any well-behaved script's response.
 */
static const size_t CGI_OUTPUT_LIMIT = 10 * 1024 * 1024;

/*
 * Anchors a path to the current working directory when it is relative, and
 * returns it unchanged when it is already absolute. The child chdir()s into the
 * directory holding the script before exec'ing, so an interpreter written
 * relative to the server's working directory ("eval_tests/cgi_tester") no
 * longer resolves from there: execve() fails, the child exits non-zero, and
 * every CGI run is answered 502. Resolving it here, in the parent, keeps the
 * path valid across the chdir.
 */
static std::string absolutePath(const std::string &path)
{
	char	cwd[PATH_MAX];

	if (!path.empty() && path[0] == '/')
		return (path);
	if (getcwd(cwd, sizeof(cwd)) == NULL)
		return (path);
	return (std::string(cwd) + "/" + path);
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
 * Runs in the child after fork: puts SIGPIPE back to its default, redirects the
 * pipe ends onto stdin/stdout, closes the leftover pipe fds, moves into the
 * directory holding the script so it can reach its own files by relative path,
 * then execve's the interpreter with the script name as argv[1] and the
 * prepared CGI environment. Never returns; _exit is called if any step fails.
 *
 * The default has to be restored because an ignored signal stays ignored
 * through execve: the child would otherwise run the script under the server's
 * own disposition, and a script still writing after the server has closed its
 * output pipe would see write() fail over and over instead of dying, spinning
 * until the CGI deadline kills it. A script runs with the dispositions any
 * other program is started with.
 */
static void runChild(CgiPipes &pipes, const std::string &interpreter, const std::string &scriptPath, char **envp)
{
	char					*argv[3];
	std::string				script = scriptPath;
	std::string				scriptFilename;
	std::string				pathTranslated;
	std::string::size_type	slash = scriptPath.find_last_of('/');

	signal(SIGPIPE, SIG_DFL);
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
	  _writing(false), _reading(false), _reaped(true), _overflow(false),
	  _deadlineMs(0)
{
}

CgiProcess::~CgiProcess(void)
{
	terminate();
}

/*
 * Takes over body as the payload to feed the child, leaving the caller's string
 * empty. Called instead of copying it in through the constructor when the
 * caller has no further use for it, so an upload on its way to a script exists
 * once rather than twice.
 */
void CgiProcess::adoptBody(std::string &body)
{
	_body.swap(body);
}

/*
 * Hands the collected output to out, leaving this process with none. The event
 * loop turns the output into a response by consuming it, so taking it over here
 * keeps a large CGI body from being held by the process and the response at the
 * same time.
 */
void CgiProcess::swapOutput(std::string &out)
{
	_output.swap(out);
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
	std::string			interpreterPath = absolutePath(interpreter);

	for (size_t i = 0; i < env.size(); ++i)
		envp.push_back(const_cast<char *>(env[i].c_str()));
	envp.push_back(NULL);
	if (!_pipes.create())
		return (false);
	_pid = fork();
	if (_pid == -1)
		return (false);
	if (_pid == 0)
		runChild(_pipes, interpreterPath, scriptPath, &envp[0]);
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
 *
 * A script that keeps writing past CGI_OUTPUT_LIMIT is cut off here rather
 * than kept reading: the accumulated output is dropped immediately (it will
 * never be served, so there is no reason to keep holding it), the read end is
 * closed, and the overflow flag tells the event loop to kill the child rather
 * than wait for it to finish on its own.
 */
void CgiProcess::onReadable(void)
{
	char	buffer[4096];
	ssize_t	bytes = read(_pipes.outputReadFd(), buffer, sizeof(buffer));

	if (bytes > 0)
	{
		_output.append(buffer, static_cast<size_t>(bytes));
		if (_output.size() > CGI_OUTPUT_LIMIT)
		{
			std::string().swap(_output);
			_overflow = true;
			_pipes.closeOutputRead();
			_reading = false;
		}
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
 *
 * The body is released here rather than at destruction: nothing reads it once
 * the child's stdin is closed, and holding it until the run ends means a large
 * upload and the equally large output the script echoes back are both resident
 * for the rest of the run.
 */
void CgiProcess::stopWriting(void)
{
	_pipes.closeBodyWrite();
	_writing = false;
	std::string().swap(_body);
	_sent = 0;
}

/*
 * Reaps the child once its output is drained and returns its exit status, or
 * -1 when it was already reaped, waitpid failed, or it did not exit normally.
 *
 * Called once both pipe directions have finished, which almost always means
 * the child has already exited: closing its own end of both pipes (whether by
 * exiting or by explicitly calling close()) is what let onReadable/onWritable
 * reach that state in the first place, so waitpid is tried first with
 * WNOHANG rather than assumed to succeed at once. A child that closed its
 * pipes without exiting is no longer honouring the CGI contract the moment
 * the server considers the exchange finished, and is killed outright rather
 * than waited on: a blocking wait() here would otherwise be able to stall the
 * entire single-threaded event loop, with nothing bounding how long that
 * child keeps running. SIGKILL cannot be caught or blocked, so the wait that
 * follows it returns essentially at once.
 */
int CgiProcess::reap(void)
{
	int	status;
	int	result;

	if (_reaped)
		return (-1);
	result = waitpid(_pid, &status, WNOHANG);
	if (result == 0)
	{
		::kill(_pid, SIGKILL);
		result = waitpid(_pid, &status, 0);
	}
	if (result <= 0)
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

bool CgiProcess::outputOverflowed(void) const
{
	return (_overflow);
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

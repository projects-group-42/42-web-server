/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   CgiHandler.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dajesus- <dajesus-@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/08 00:00:00 by dajesus-          #+#    #+#             */
/*   Updated: 2026/07/08 00:00:00 by dajesus-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "cgi/CgiHandler.hpp"
#include "cgi/CgiPipes.hpp"
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <poll.h>
#include <fcntl.h>
#include <errno.h>

CgiHandler::CgiHandler(void)
	: _cgiRoot("cgi-bin")
{
}

CgiHandler::CgiHandler(const std::string &cgiRoot)
	: _cgiRoot(cgiRoot)
{
}

CgiHandler::CgiHandler(const CgiHandler &copy)
{
	*this = copy;
}

CgiHandler &CgiHandler::operator=(const CgiHandler &other)
{
	if (this != &other)
		_cgiRoot = other._cgiRoot;
	return (*this);
}

CgiHandler::~CgiHandler(void)
{
}

void	CgiHandler::setCgiRoot(const std::string &cgiRoot)
{
	_cgiRoot = cgiRoot;
}

const std::string &CgiHandler::getCgiRoot(void) const
{
	return (_cgiRoot);
}

/*
 * Returns true if uri ends with the given extension.
 */
bool CgiHandler::hasExtension(const std::string &uri,
		const std::string &extension) const
{
	if (uri.size() < extension.size())
		return (false);
	return (uri.compare(uri.size() - extension.size(),
			extension.size(), extension) == 0);
}

/*
 * Returns the canonical absolute path of path, or an empty string when it
 * cannot be resolved (for instance because it does not exist).
 */
static std::string canonicalPath(const std::string &path)
{
	char	buffer[PATH_MAX];

	if (realpath(path.c_str(), buffer) == NULL)
		return ("");
	return (std::string(buffer));
}

/*
 * Splits uri into normalized path segments. Returns false when a
 * ".." would climb above the root, true otherwise.
 */
static bool normalizeSegments(const std::string &uri,
		std::vector<std::string> &segments)
{
	size_t	counter = 0;
	size_t	start;
	std::string	segment;

	while (counter < uri.size())
	{
		while (counter < uri.size() && uri[counter] == '/')
		    ++counter;
		start = counter;

		while (counter < uri.size() && uri[counter] != '/')
		    ++counter;
		if (counter == start)
			continue;

		segment = uri.substr(start, counter - start);
		if (segment == ".")
			continue;
		if (segment == "..")
		{
			if (segments.empty())
				return (false);
			segments.pop_back();
		}
		else
			segments.push_back(segment);
	}
	return (true);
}

/*
 * Joins root and the normalized segments into a single slash-separated path.
 */
static std::string joinPath(const std::string &root,
		const std::vector<std::string> &segments)
{
	std::string	path = root;

	for (size_t counter = 0; counter < segments.size(); ++counter)
		path += "/" + segments[counter];
	return (path);
}

/*
 * Returns true when path canonically resolves inside root, so symlinks
 * cannot escape it. Paths that cannot be canonicalized are treated as inside.
 */
static bool isWithinRoot(const std::string &root, const std::string &path)
{
	std::string	canonicalRoot = canonicalPath(root);
	std::string	resolved = canonicalPath(path);

	if ((canonicalRoot.empty() || resolved.empty()) || resolved == canonicalRoot)
		return (true);
	return (resolved.compare(0, canonicalRoot.size() + 1, canonicalRoot + "/") == 0);
}

/*
 * Returns the last path component of path, ignoring trailing slashes.
 */
static std::string baseName(const std::string &path)
{
	std::string	trimmed = path;

	while (trimmed.size() > 1 && trimmed[trimmed.size() - 1] == '/')
		trimmed.erase(trimmed.size() - 1);
	std::string::size_type	slash = trimmed.rfind('/');
	if (slash == std::string::npos)
		return (trimmed);
	return (trimmed.substr(slash + 1));
}

/*
 * Resolves the script path for a URI under the CGI root. The URI is
 * normalized, its leading mount segment (matching the CGI root name) is
 * dropped so "/cgi-bin/x.py" maps to "<root>/x.py", the rest is joined onto
 * the root, and the result is checked so symlinks cannot escape it. Returns an
 * empty string on escape.
 */
std::string CgiHandler::resolvePath(const std::string &uri) const
{
	std::vector<std::string>	segments;

	if (!normalizeSegments(uri, segments))
		return ("");
	if (!segments.empty() && segments.front() == baseName(_cgiRoot))
		segments.erase(segments.begin());
	std::string	path = joinPath(_cgiRoot, segments);
	if (!isWithinRoot(_cgiRoot, path))
		return ("");
	return (path);
}

/*
 * Runs in the child after fork: redirects the pipe ends onto stdin/stdout,
 * closes the leftover pipe fds, then execve's the interpreter with the script
 * as argv[1] and the prepared CGI environment. Never returns; _exit is called
 * if any step fails.
 */
static void runCgiChild(CgiPipes &pipes, const std::string &interpreter, const std::string &scriptPath, char **envp)
{
    char    *argv[3];

    pipes.closeParentEnds();
    if(dup2(pipes.bodyReadFd(), STDIN_FILENO) == -1)
        _exit(1);
    if(dup2(pipes.outputWriteFd(), STDOUT_FILENO) == -1)
        _exit(1);
    pipes.closeChildEnds();

    argv[0] = const_cast<char *>(interpreter.c_str());
    argv[1] = const_cast<char *>(scriptPath.c_str());
    argv[2] = NULL;

    execve(interpreter.c_str(), argv, envp);
    _exit(1);
}

/*
 * Puts fd into non-blocking mode so a write can never stall the parent.
 * Returns false when the current flags cannot be read or updated.
 */
static bool setNonBlocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);

    if (flags == -1)
        return (false);
    return (fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1);
}

/*
 * Feeds body to the child's stdin while draining its stdout at the same time,
 * multiplexing both pipes with poll so a body larger than the pipe buffer
 * cannot deadlock the parent. Returns false only on a poll or read error.
 */
static bool pumpCgiIo(CgiPipes &pipes, const std::string &body, std::string &output)
{
    struct pollfd   fds[2];
    size_t          sent = 0;
    bool            writing = !body.empty();
    bool            reading = true;

    if (!writing)
        pipes.closeBodyWrite();
    else
        setNonBlocking(pipes.bodyWriteFd());
    while (reading || writing)
    {
        nfds_t  count = 0;
        int     outIndex = -1;
        int     bodyIndex = -1;

        if (reading)
        {
            fds[count].fd = pipes.outputReadFd();
            fds[count].events = POLLIN;
            fds[count].revents = 0;
            outIndex = static_cast<int>(count);
            ++count;
        }
        if (writing)
        {
            fds[count].fd = pipes.bodyWriteFd();
            fds[count].events = POLLOUT;
            fds[count].revents = 0;
            bodyIndex = static_cast<int>(count);
            ++count;
        }
        if (poll(fds, count, -1) == -1)
        {
            if (errno == EINTR)
                continue;
            return (false);
        }
        if (writing && (fds[bodyIndex].revents & (POLLERR | POLLHUP)))
        {
            pipes.closeBodyWrite();
            writing = false;
        }
        else if (writing && (fds[bodyIndex].revents & POLLOUT))
        {
            ssize_t written = write(pipes.bodyWriteFd(), body.data() + sent, body.size() - sent);

            if (written == -1)
            {
                if (errno != EAGAIN && errno != EWOULDBLOCK)
                {
                    pipes.closeBodyWrite();
                    writing = false;
                }
            }
            else
            {
                sent += static_cast<size_t>(written);
                if (sent == body.size())
                {
                    pipes.closeBodyWrite();
                    writing = false;
                }
            }
        }
        if (reading && (fds[outIndex].revents & (POLLIN | POLLHUP | POLLERR)))
        {
            char    buffer[4096];
            ssize_t bytes = read(pipes.outputReadFd(), buffer, sizeof(buffer));

            if (bytes > 0)
                output.append(buffer, static_cast<size_t>(bytes));
            else if (bytes == 0)
            {
                pipes.closeOutputRead();
                reading = false;
            }
            else
                return (false);
        }
    }
    return (true);
}

bool CgiHandler::isCgiRequest(const std::string &uri) const
{
	return (hasExtension(uri, ".py"));
}

/*
 * Validates the script resolved from the URI (path stays inside the CGI
 * root, exists, is a regular file, is readable) and writes the resolved path
 * into scriptPath on success. Sets the response status code and returns false
 * when validation fails.
 */
bool CgiHandler::validate(const std::string &uri, std::string &scriptPath,
		HttpResponse &response) const
{
	scriptPath = resolvePath(uri);

	if (scriptPath.empty())
	{
		response.setStatusCode(403);
		return (false);
	}

	struct stat	scriptStat;
	if (stat(scriptPath.c_str(), &scriptStat) != 0)
	{
		response.setStatusCode(404);
		return (false);
	}
	if (!S_ISREG(scriptStat.st_mode))
	{
		response.setStatusCode(403);
		return (false);
	}
	if (access(scriptPath.c_str(), R_OK) != 0)
	{
		response.setStatusCode(403);
		return (false);
	}
	return (true);
}

/*
 * Converts a decimal value into its string representation.
 */
static std::string toString(size_t value)
{
    std::ostringstream  stream;

    stream << value;
    return (stream.str());
}

/*
 * Builds the CGI meta-variable name for an HTTP header: uppercases the key,
 * replaces every '-' with '_', and prefixes it with "HTTP_".
 */
static std::string headerToMetaVar(const std::string &key)
{
    std::string name = "HTTP_";

    for (size_t i = 0; i < key.size(); ++i)
    {
        char c = key[i];
        if (c == '-')
            name += '_';
        else if (c >= 'a' && c <= 'z')
            name += static_cast<char>(c - 'a' + 'A');
        else
            name += c;
    }
    return (name);
}

/*
 * Builds the CGI environment for a request and the
 * resolved script path. Includes the request method, query string, protocol
 * and content metadata, and forwards every request header as an HTTP_ variable
 * except the ones already exposed as CONTENT_TYPE and CONTENT_LENGTH.
 */
std::vector<std::string> CgiHandler::buildEnv(const HttpRequest &request, const std::string &scriptPath) const
{
    std::vector<std::string>    env;
    std::string                 protocol = request.getVersion();
    std::string                 contentType = request.getHeaderValue("Content-Type");

    if (protocol.empty())
        protocol = "HTTP/1.1";
    env.push_back("GATEWAY_INTERFACE=CGI/1.1");
    env.push_back("SERVER_SOFTWARE=Webserv/1.0");
    env.push_back("SERVER_PROTOCOL=" + protocol);
    env.push_back("REDIRECT_STATUS=200");
    env.push_back("REQUEST_METHOD=" + request.getMethod());
    env.push_back("QUERY_STRING=" + request.getQuery());
    env.push_back("SCRIPT_NAME=" + request.getUri());
    env.push_back("SCRIPT_FILENAME=" + scriptPath);
    env.push_back("CONTENT_LENGTH=" + toString(request.getBody().size()));
    if (!contentType.empty())
        env.push_back("CONTENT_TYPE=" + contentType);
    const std::map<std::string, std::string> &headers = request.getHeaders();
    for (std::map<std::string, std::string>::const_iterator it = headers.begin();
            it != headers.end(); ++it)
    {
        if (it->first == "content-type" || it->first == "content-length")
            continue;
        env.push_back(headerToMetaVar(it->first) + "=" + it->second);
    }
    return (env);
}

/*
 * Strips leading and trailing spaces, tabs and a trailing carriage return from
 * a CGI header value.
 */
static std::string trimHeaderValue(const std::string &value)
{
    size_t  start = 0;
    size_t  end = value.size();

    while (start < end && (value[start] == ' ' || value[start] == '\t'))
        ++start;
    while (end > start && (value[end - 1] == ' ' || value[end - 1] == '\t'
            || value[end - 1] == '\r'))
        --end;
    return (value.substr(start, end - start));
}

/*
 * Case-insensitive check for the CGI "Status" header name.
 */
static bool isStatusHeader(const std::string &key)
{
    static const char   name[] = "status";

    if (key.size() != sizeof(name) - 1)
        return (false);
    for (size_t i = 0; i < key.size(); ++i)
    {
        char    c = key[i];
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
        if (c != name[i])
            return (false);
    }
    return (true);
}

/*
 * Returns true when c is a valid HTTP token character, the only characters
 * allowed in a header field name.
 */
static bool isTokenChar(char c)
{
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
            || (c >= '0' && c <= '9'))
        return (true);
    return (std::string("!#$%&'*+-.^_`|~").find(c) != std::string::npos);
}

/*
 * Returns true when key is a non-empty, well-formed header field name, so a
 * line of script noise that happens to contain a colon is not taken as a
 * header.
 */
static bool isValidHeaderName(const std::string &key)
{
    if (key.empty())
        return (false);
    for (size_t i = 0; i < key.size(); ++i)
    {
        if (!isTokenChar(key[i]))
            return (false);
    }
    return (true);
}

/*
 * Reads a CGI "Status" value: exactly three digits in the 100-599 range,
 * optionally followed by a reason phrase. Writes the code into status and
 * returns true on success, false when the value is malformed.
 */
static bool parseStatusValue(const std::string &value, int &status)
{
    size_t  i = 0;
    int     code = 0;

    while (i < value.size() && value[i] >= '0' && value[i] <= '9')
    {
        code = code * 10 + (value[i] - '0');
        ++i;
    }
    if (i != 3)
        return (false);
    if (i < value.size() && value[i] != ' ' && value[i] != '\t')
        return (false);
    if (code < 100 || code > 599)
        return (false);
    status = code;
    return (true);
}

/*
 * Converts the raw CGI output into an HttpResponse: splits the header block
 * from the body at the first blank line, maps each "Key: Value" line into a
 * response header, consumes the "Status" header into the status code (default
 * 200), and stores the remaining bytes as the body. Returns false and leaves
 * response untouched when the script produced no output, no header separator,
 * an empty header section, a line that is not a valid header, or a malformed
 * Status value, so the caller can answer 502 instead of serving the garbage.
 */
bool CgiHandler::parseCgiOutput(const std::string &raw, HttpResponse &response) const
{
    std::string::size_type  sep = raw.find("\r\n\r\n");
    std::string::size_type  sepLen = 4;

    if (raw.empty())
        return (false);
    if (sep == std::string::npos)
    {
        sep = raw.find("\n\n");
        sepLen = 2;
    }
    if (sep == std::string::npos)
        return (false);

    std::istringstream                                  headers(raw.substr(0, sep));
    std::string                                         line;
    std::vector<std::pair<std::string, std::string> >   parsed;
    int                                                 statusCode = 200;
    bool                                                hasHeader = false;

    while (std::getline(headers, line))
    {
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);

        std::string::size_type  colon = line.find(':');
        if (colon == std::string::npos)
            return (false);

        std::string key = line.substr(0, colon);
        std::string value = trimHeaderValue(line.substr(colon + 1));
        if (!isValidHeaderName(key))
            return (false);
        if (isStatusHeader(key))
        {
            if (!parseStatusValue(value, statusCode))
                return (false);
        }
        else
            parsed.push_back(std::make_pair(key, value));
        hasHeader = true;
    }
    if (!hasHeader)
        return (false);

    response.setStatusCode(statusCode);
    for (size_t i = 0; i < parsed.size(); ++i)
        response.addHeader(parsed[i].first, parsed[i].second);
    response.setBody(raw.substr(sep + sepLen));
    return (true);
}

/*
 * Runs the CGI script through the interpreter: creates the pipes, forks, wires
 * the child's stdin/stdout to the pipes, passes env as the child's environment,
 * then streams body to the child while collecting its stdout into output at the
 * same time. Reaps the child and returns false on fork/pipe failure, on I/O
 * error, or when the script does not exit cleanly with status 0.
 */
bool    CgiHandler::execute(const std::string &interpreter, const std::string &scriptPath, const std::string &body, const std::vector<std::string> &env, std::string &output) const
{
    CgiPipes                pipes;
    std::vector<char *>     envp;
    pid_t                   pid;
    bool                    ok;
    int                     status;

    for (size_t i = 0; i < env.size(); ++i)
        envp.push_back(const_cast<char *>(env[i].c_str()));
    envp.push_back(NULL);
    if (!pipes.create())
        return(false);
    pid = fork();
    if (pid == -1)
        return(false);
    if (pid == 0)
        runCgiChild(pipes, interpreter, scriptPath, &envp[0]);
    pipes.closeChildEnds();
    ok = pumpCgiIo(pipes, body, output);
    if (waitpid(pid, &status, 0) == -1)
        return (false);
    if (!ok)
        return (false);
    if (!WIFEXITED(status))
        return (false);
    if (WEXITSTATUS(status) != 0)
        return (false);
    return (true);
}

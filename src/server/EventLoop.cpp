/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   EventLoop.cpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/04 19:05:52 by jucoelho          #+#    #+#             */
/*   Updated: 2026/08/07 01:29:29 by galves-a         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "server/EventLoop.hpp"
#include "http/ResponseBuilder.hpp"
#include "utils/Logger.hpp"
#include "utils/Utils.hpp"
#include <unistd.h>
#include <cerrno>
#include <stdexcept>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <ctime>
#include <cstdlib>

static const std::string	DEFAULT_CGI_INTERPRETER = "/usr/bin/python3";
static const long			CGI_TIMEOUT_MS = 5000;
static const double			IDLE_TIMEOUT_S = 30.0;
static const int			BACKLOG = 128;

/*
 * Returns the current wall-clock time in milliseconds. gettimeofday() is not
 * one of the functions the subject authorises, so the value comes from
 * std::time and carries a resolution of one second: the only deadline built on
 * it is the CGI timeout, which then fires between four and five seconds after
 * the child started instead of exactly five.
 */
static long nowMs(void)
{
	return (static_cast<long>(std::time(NULL)) * 1000);
}

EventLoop::EventLoop(void) : _configs(), _router(DEFAULT_ROOT)
{
}

EventLoop::EventLoop(const std::vector<ServerConfig> &configs)
	: _configs(configs), _router(DEFAULT_ROOT)
{
}

EventLoop::~EventLoop(void)
{
	for (size_t i = 0; i < _sckt.size(); i++)
		delete _sckt[i];
}

/*
 * Reports whether a listening socket is already bound to `host:port`. Server
 * blocks sharing an interface and a port share the socket, and the block
 * serving a request is picked from its Host header. The interface is part of
 * the identity: two blocks on the same port but on different addresses are two
 * sockets, and comparing the port alone silently dropped the second one.
 */
bool EventLoop::isEndpointBound(const std::string &host, int port) const
{
	for (size_t i = 0; i < _boundEndpoints.size(); i++)
	{
		if (_boundEndpoints[i].second == port
			&& _boundEndpoints[i].first == host)
			return (true);
	}
	return (false);
}

/*
 * Returns the widest client_max_body_size any server block or location on
 * `port` allows. The block and location serving a request are only known once
 * its headers have been read, so the parser is armed with the most permissive
 * value on the port: it is the largest body that could possibly be accepted
 * there, and anything past it can be refused while it is still being read.
 * A port of 0, which is what getLocalPort() reports when it cannot query the
 * socket, matches no block and widens the search to every block instead, so an
 * unreadable socket cannot narrow the limit into a spurious 413.
 */
long EventLoop::maxBodySizeForPort(int port) const
{
	long	widest = -1;

	for (size_t i = 0; i < _configs.size(); i++)
	{
		const ServerConfig	&config = _configs[i];

		if (port != 0 && config.port != port)
			continue;
		if (config.clientMaxBodySize > widest)
			widest = config.clientMaxBodySize;
		for (size_t j = 0; j < config.locations.size(); j++)
		{
			if (config.locations[j].clientMaxBodySize > widest)
				widest = config.locations[j].clientMaxBodySize;
		}
	}
	if (widest < 0)
		return (DEFAULT_MAX_BODY_SIZE);
	return (widest);
}

/*
 * Opens one listening socket per distinct port declared in the config.
 * The socket is registered before it is configured so a failure part way
 * through still leaves it owned by the loop and freed by the destructor.
 */
void EventLoop::setupSockets(void)
{
	if (_configs.empty())
		throw std::runtime_error("EventLoop: no server configuration");

	for (size_t i = 0; i < _configs.size(); i++)
	{
		const std::string	&host = _configs[i].host;
		int					port = _configs[i].port;

		if (isEndpointBound(host, port))
			continue;

		_sckt.push_back(new Socket());

		Socket	*sckt = _sckt.back();

		sckt->create();
		sckt->bind(host, port);
		sckt->listen(BACKLOG);

		std::ostringstream	oss;
		oss << "Listening on " << host << ":" << port;
		Logger::info(oss.str());

		_boundEndpoints.push_back(std::make_pair(host, port));
	}
}

/*
 * Reports whether `fd` is one of the listening sockets rather than a client.
 */
bool EventLoop::isMasterSocket(int fd) const
{
	for (size_t i = 0; i < _sckt.size(); i++)
	{
		if (_sckt[i]->getFd() == fd)
			return (true);
	}
	return (false);
}

/*
 * Formats the IPv4 address of a peer as a dotted quad. inet_ntoa() is not one
 * of the functions the subject authorises, and the four octets are all the CGI
 * environment needs for REMOTE_ADDR.
 */
static std::string addressToString(const struct sockaddr_in &address)
{
	unsigned long		host = ntohl(address.sin_addr.s_addr);
	std::ostringstream	oss;

	oss << ((host >> 24) & 0xFF) << "." << ((host >> 16) & 0xFF) << "."
		<< ((host >> 8) & 0xFF) << "." << (host & 0xFF);
	return (oss.str());
}

void EventLoop::acceptClients(int fd)
{
	while (true)
	{
		struct sockaddr_in	peer;
		socklen_t			length = sizeof(peer);
		int client = accept(fd, (struct sockaddr *)&peer, &length);
		if (client == -1)
			break;
		setNonBlocking(client);
		struct pollfd pfd;
		pfd.fd = client;
		pfd.events = POLLIN;
		pfd.revents = 0;
		_fds.push_back(pfd);
		_clients[client] = Connection(client, addressToString(peer));
		_clients[client].setMaxBodySize(
			maxBodySizeForPort(_clients[client].getLocalPort()));
		Logger::info("New client connected.");
	}
}

/*
 * Reads one ready chunk from a client. The return value of recv() alone decides
 * what happens: anything but a positive count ends the connection, since 0 is
 * the peer closing and -1 is an error on a socket poll() had just reported as
 * readable. Returning false makes run() drop the client and its poll entry.
 */
bool EventLoop::handleClient(int fd)
{
	ssize_t	n = _clients[fd].receive_data();

	if (n == 0)
	{
		Logger::info("Client closed the connection.");
		return (false);
	}
	if (n < 0)
	{
		Logger::error("recv() failed, dropping client.");
		return (false);
	}
	Logger::info("Data received from client.");
	if (_clients[fd].get_psr_state() == COMPLETE)
		handleRequest(fd);
	else if (_clients[fd].get_psr_state() == ERROR)
		handleParseError(fd);
	return (true);
}

/**
 * @brief Serialises an error response using the configured error page.
 * Picks the server block matching the port and Host header of the connection
 * and resolves its error_page for the status. Falls back to the built-in body
 * when no page is configured or the file cannot be read.
 * @param conn The connection being answered.
 * @param builder The builder carrying the keep-alive state of the connection.
 * @param status The status to answer with.
 * @return The serialised response, headers included.
 */
std::string EventLoop::buildError(const Connection &conn,
	const ResponseBuilder &builder, int status) const
{
	std::string	body;
	std::string	contentType;

	if (!_configs.empty())
	{
		const ServerConfig	&config = getServerConfigForRequest(
									conn.getLocalPort(), conn.getRequest());

		if (!Router::loadErrorPage(config, config.root, status, body,
				contentType))
		{
			body.clear();
			contentType.clear();
		}
	}
	return (builder.buildErrorResponse(status, body, contentType));
}

/*
 * Answers a request the parser refused. The connection is marked as closing:
 * a request that could not be framed says nothing about where the next one
 * begins, so the bytes still to come cannot be trusted to be a request of their
 * own. The error response already advertises "Connection: close", since it is
 * serialised by a builder that was never told the connection is persistent, so
 * a connection kept alive by an earlier request has to be closed here to match
 * what the client is being told.
 */
void EventLoop::handleParseError(int fd)
{
	Connection		&conn = _clients[fd];
	ResponseBuilder	builder;

	int error_code = conn.get_error_code();
	if (error_code == 0)
		error_code = 400;

	conn.set_keep_alive(false);
	conn.set_write_buffer(buildError(conn, builder, error_code));

	setPollEvents(fd, POLLOUT);
}

/*
 * Decides whether the client wants the connection kept open. HTTP/1.1
 * defaults to persistent unless "Connection: close" is sent; HTTP/1.0
 * defaults to closing unless "Connection: keep-alive" is sent.
 */
bool EventLoop::wantsKeepAlive(const HttpRequest &request) const
{
	std::string connection = toLower(request.getHeaderValue("connection"));

	if (connection == "close")
		return (false);
	if (request.getVersion() == "HTTP/1.0")
		return (connection == "keep-alive");
	return (true);
}

/*
 * Updates the poll interest (POLLIN/POLLOUT) for the given client fd.
 */
void EventLoop::setPollEvents(int fd, short events)
{
	for (size_t i = 0; i < _fds.size(); i++)
	{
		if (_fds[i].fd == fd)
		{
			_fds[i].events = events;
			break;
		}
	}
}

void EventLoop::handleRequest(int fd)
{
	Connection			&conn = _clients[fd];
	ResponseBuilder		builder;
	const ServerConfig	&chosenConfig = getServerConfigForRequest(
								conn.getLocalPort(), conn.getRequest());

	conn.set_keep_alive(wantsKeepAlive(conn.getRequest()));
	builder.setKeepAlive(conn.get_keep_alive());

	if (_router.bodyExceedsLimit(conn.getRequest(), chosenConfig))
	{
		Logger::error("413 Content Too Large");
		conn.set_write_buffer(buildError(conn, builder, 413));
		setPollEvents(fd, POLLOUT);
		return ;
	}

	std::string	interpreter = cgiInterpreterFor(conn.getRequest(),
						chosenConfig);

	if (!interpreter.empty())
	{
		startCgi(fd, interpreter, chosenConfig);
		return ;
	}

	try
	{
		HttpResponse response;
		_router.route(conn.getRequest(), response, chosenConfig);
		std::string serialized = builder.builder(conn.getRequest(), response);
		conn.set_write_buffer(serialized);
	}
	catch (std::exception &e)
	{
		Logger::error(std::string("Internal Server Error: ") + e.what());
		conn.set_write_buffer(buildError(conn, builder, 500));
	}
	catch (...)
	{
		Logger::error("Internal Server Error: unknown exception");
		conn.set_write_buffer(buildError(conn, builder, 500));
	}
	setPollEvents(fd, POLLOUT);
}

/*
 * Writes what is pending on the connection and decides what follows once the
 * response is out. Resetting a kept-alive connection re-parses the bytes read
 * along with the request just answered, so the next request may already be
 * complete, or already refused: a pipelined request is parsed here rather than
 * in handleClient, and a parser error found at this point has to be answered
 * here too, or the client is left waiting on a response that is never written.
 *
 * As in handleClient, the return value of send() alone decides: a write that
 * moved no byte (0) or failed (-1) on a socket poll() had reported as writable
 * ends the connection. send() is only called while bytes are actually pending,
 * so an empty buffer never produces a 0 of its own.
 */
bool EventLoop::handleSend(int fd)
{
	Connection	&conn = _clients[fd];

	if (conn.has_data_to_send())
	{
		ssize_t	sent = conn.send_data();

		if (sent <= 0)
		{
			Logger::error("send() made no progress, dropping client.");
			return (false);
		}
		if (conn.has_data_to_send())
			return (true); // more to send
	}
	if (!conn.get_keep_alive())
	{
		Logger::info("Response fully sent, closing connection.");
		return (false); // done, close
	}
	conn.reset_for_next_request();
	setPollEvents(fd, POLLIN);
	Logger::info("Response fully sent, keeping connection alive.");
	if (conn.get_psr_state() == COMPLETE)
		handleRequest(fd);
	else if (conn.get_psr_state() == ERROR)
		handleParseError(fd);
	return (true);
}

/*
 * Appends a new pollfd for fd with the given interest to the poll set.
 */
void EventLoop::addPollFd(int fd, short events)
{
	struct pollfd	pfd;

	pfd.fd = fd;
	pfd.events = events;
	pfd.revents = 0;
	_fds.push_back(pfd);
}

/*
 * Marks fd's poll entry as inactive by setting its fd to -1, which poll()
 * ignores. The entry is removed later by compactPollFds so the set is never
 * resized while the run() loop is iterating it.
 */
void EventLoop::disablePollFd(int fd)
{
	if (fd < 0)
		return ;
	for (size_t i = 0; i < _fds.size(); ++i)
	{
		if (_fds[i].fd == fd)
		{
			_fds[i].fd = -1;
			_fds[i].events = 0;
			break;
		}
	}
}

/*
 * Drops every poll entry disabled during the current iteration.
 */
void EventLoop::compactPollFds(void)
{
	for (size_t i = 0; i < _fds.size(); )
	{
		if (_fds[i].fd == -1)
			_fds.erase(_fds.begin() + i);
		else
			++i;
	}
}

/*
 * Queues an error response on the client and arms it for sending. Used when a
 * CGI request cannot be started (invalid script or fork failure).
 */
void EventLoop::sendCgiError(int fd, int status)
{
	Connection		&conn = _clients[fd];
	ResponseBuilder	builder;

	builder.setKeepAlive(conn.get_keep_alive());
	conn.set_write_buffer(buildError(conn, builder, status));
	setPollEvents(fd, POLLOUT);
}

/**
 * @brief Decides whether a URI is a CGI request and which binary runs it.
 * The cgi_pass directives of the location matching the URI answer both at
 * once, so a script only executes when the config declares a handler for its
 * extension and any other extension keeps being served as a static file. A
 * ".py" script no location binds falls back to the default interpreter, so a
 * config declaring no cgi_pass still serves Python scripts. A location that
 * redirects runs nothing, since the redirect is the answer and the fallback
 * would otherwise execute the script before the router ever sees the request.
 * A method the location does not accept runs nothing either, for the same
 * reason: the 405 is the answer, and it is the router that writes it.
 * @param request The request being answered.
 * @param config The server block serving the request.
 * @return The binary to execute, or an empty string when the URI is not a CGI
 * request.
 */
std::string	EventLoop::cgiInterpreterFor(const HttpRequest &request,
			const ServerConfig &config) const
{
	const std::string	&uri = request.getUri();

	if (_router.redirects(uri, config) || _router.refusesMethod(request, config))
		return ("");

	std::string	interpreter = _router.resolveCgiInterpreter(uri, config);

	if (interpreter.empty() && _cgiHandler.isCgiRequest(uri))
		return (DEFAULT_CGI_INTERPRETER);
	return (interpreter);
}

/*
 * Starts a CGI request without blocking the server: points the CGI handler at
 * the root the matched location declares, validates the script, forks the
 * interpreter the config bound to its extension, registers the CGI pipe fds in
 * the poll set, and parks the client fd (no interest) until the child
 * finishes. On validation or fork failure it queues the matching error
 * response instead.
 */
void EventLoop::startCgi(int fd, const std::string &interpreter,
		const ServerConfig &config)
{
	Connection		&conn = _clients[fd];
	std::string		scriptPath;
	HttpResponse	errorResponse;
	const std::string	&uri = conn.getRequest().getUri();

	_cgiHandler.setCgiRoot(_router.resolveRoot(uri, config));
	_cgiHandler.setLocationPrefix(_router.resolveLocationPrefix(uri, config));
	if (!_cgiHandler.validate(uri, scriptPath, errorResponse))
	{
		sendCgiError(fd, errorResponse.getStatusCode());
		return ;
	}

	std::vector<std::string>	env = _cgiHandler.buildEnv(conn.getRequest(),
									scriptPath, conn.getLocalPort(),
									conn.getRemoteAddr());
	CgiProcess					*proc = new CgiProcess(fd, conn.getRequest().getBody());

	if (!proc->start(interpreter, scriptPath, env))
	{
		delete proc;
		sendCgiError(fd, 500);
		return ;
	}
	proc->setDeadlineMs(nowMs() + CGI_TIMEOUT_MS);
	_cgi[fd] = proc;
	_pipeToClient[proc->outputReadFd()] = fd;
	addPollFd(proc->outputReadFd(), POLLIN);
	if (proc->isWriting())
	{
		_pipeToClient[proc->bodyWriteFd()] = fd;
		addPollFd(proc->bodyWriteFd(), POLLOUT);
	}
	setPollEvents(fd, 0);
	Logger::info("CGI started, server stays responsive.");
}

/*
 * Advances one CGI pipe by a single non-blocking step. Writable steps feed the
 * request body to the child; readable steps drain its output. A finished
 * direction is unregistered from the poll set, and once both directions are
 * done the response is built and sent.
 */
void EventLoop::handleCgiIo(int fd, short revents)
{
	int			clientFd = _pipeToClient[fd];
	CgiProcess	*proc = _cgi[clientFd];

	if (fd == proc->bodyWriteFd())
	{
		if (revents & (POLLERR | POLLHUP | POLLNVAL))
			proc->stopWriting();
		else if (revents & POLLOUT)
			proc->onWritable();
		if (!proc->isWriting())
		{
			disablePollFd(fd);
			_pipeToClient.erase(fd);
		}
	}
	else
	{
		if (revents & (POLLIN | POLLHUP | POLLERR))
			proc->onReadable();
		if (!proc->isReading())
		{
			disablePollFd(fd);
			_pipeToClient.erase(fd);
		}
	}
	if (proc->finished())
		finishCgi(clientFd, proc);
}

/*
 * Reaps the finished child, turns its collected output into an HTTP response,
 * arms the client for sending, and releases the process. Answers 502 when the
 * script did not exit cleanly or when its output is not a valid CGI response.
 */
void EventLoop::finishCgi(int clientFd, CgiProcess *proc)
{
	Connection		&conn = _clients[clientFd];
	ResponseBuilder	builder;
	HttpResponse	response;
	int				status = proc->reap();

	builder.setKeepAlive(conn.get_keep_alive());
	if (status == 0 && _cgiHandler.parseCgiOutput(proc->output(), response))
		conn.set_write_buffer(builder.builder(conn.getRequest(), response));
	else
		conn.set_write_buffer(buildError(conn, builder, 502));
	setPollEvents(clientFd, POLLOUT);
	_cgi.erase(clientFd);
	delete proc;
	Logger::info("CGI finished, response queued.");
}

/*
 * Removes every poll entry and mapping for the CGI pipes owned by clientFd.
 */
void EventLoop::unregisterCgiPipes(int clientFd)
{
	for (std::map<int, int>::iterator it = _pipeToClient.begin(); it != _pipeToClient.end(); )
	{
		if (it->second == clientFd)
		{
			disablePollFd(it->first);
			_pipeToClient.erase(it++);
		}
		else
			++it;
	}
}

/*
 * Tears down a CGI whose client disconnected mid-execution: unregisters its
 * pipe fds, kills and reaps the child, and drops the client connection.
 */
void EventLoop::abortCgi(int clientFd)
{
	CgiProcess	*proc = _cgi[clientFd];

	unregisterCgiPipes(clientFd);
	_cgi.erase(clientFd);
	delete proc;
	disablePollFd(clientFd);
	_clients.erase(clientFd);
	Logger::info("Client disconnected during CGI, process terminated.");
}

/*
 * Kills a CGI that ran past its deadline: unregisters its pipe fds, reaps the
 * child (through the CgiProcess destructor), queues a 504 Gateway Timeout, and
 * arms the client for sending.
 */
void EventLoop::timeoutCgi(int clientFd)
{
	Connection		&conn = _clients[clientFd];
	CgiProcess		*proc = _cgi[clientFd];
	ResponseBuilder	builder;

	unregisterCgiPipes(clientFd);
	_cgi.erase(clientFd);
	delete proc;
	builder.setKeepAlive(conn.get_keep_alive());
	conn.set_write_buffer(buildError(conn, builder, 504));
	setPollEvents(clientFd, POLLOUT);
	Logger::info("CGI timed out, 504 queued.");
}

/*
 * Kills every CGI whose deadline has passed. Deadlines are collected before
 * killing so the map is not modified while it is being iterated.
 */
void EventLoop::checkCgiTimeouts(void)
{
	long				now = nowMs();
	std::vector<int>	expired;

	for (std::map<int, CgiProcess*>::iterator it = _cgi.begin(); it != _cgi.end(); ++it)
	{
		if (now >= it->second->deadlineMs())
			expired.push_back(it->first);
	}
	for (size_t i = 0; i < expired.size(); ++i)
		timeoutCgi(expired[i]);
}

/*
 * Closes every connection that has been silent for longer than the idle
 * timeout. A client that opens a socket and says nothing, or stops halfway
 * through a request header, would otherwise hold its descriptor for as long as
 * the process lives. Connections waiting on a CGI are left alone: they are
 * idle by definition while the child runs, and the CGI deadline already bounds
 * them.
 */
void EventLoop::closeIdleConnections(void)
{
	std::vector<int>	expired;

	for (std::map<int, Connection>::iterator it = _clients.begin();
			it != _clients.end(); ++it)
	{
		if (_cgi.count(it->first))
			continue;
		if (it->second.last_activity() >= IDLE_TIMEOUT_S)
			expired.push_back(it->first);
	}
	for (size_t i = 0; i < expired.size(); ++i)
	{
		disablePollFd(expired[i]);
		_clients.erase(expired[i]);
		Logger::info("Idle connection closed.");
	}
}

/*
 * Returns the poll timeout in milliseconds: infinite when no CGI is running,
 * otherwise the time left until the nearest CGI deadline (never negative) so
 * poll wakes in time to kill a stuck child.
 */
int EventLoop::cgiPollTimeout(void)
{
	long	now;
	long	soonest = -1;

	if (_cgi.empty())
		return (-1);
	now = nowMs();
	for (std::map<int, CgiProcess*>::iterator it = _cgi.begin(); it != _cgi.end(); ++it)
	{
		long	remaining = it->second->deadlineMs() - now;

		if (remaining < 0)
			remaining = 0;
		if (soonest == -1 || remaining < soonest)
			soonest = remaining;
	}
	return (static_cast<int>(soonest));
}

/*
 * Returns the timeout the main poll() waits with: the nearest of the CGI
 * deadline and the next idle connection sweep. Without the second half a
 * silent client would only be noticed the next time some other traffic woke
 * the loop up.
 */
int EventLoop::pollTimeout(void)
{
	int	cgi = cgiPollTimeout();
	int	idle = static_cast<int>(IDLE_TIMEOUT_S * 1000);

	if (_clients.empty())
		return (cgi);
	if (cgi == -1 || cgi > idle)
		return (idle);
	return (cgi);
}

void EventLoop::run(void)
{
	if (_sckt.empty())
		throw std::runtime_error("EventLoop: no sockets initialized");
	for (size_t i = 0; i < _sckt.size(); i++)
	{
		struct pollfd s_listening;
		s_listening.fd = _sckt[i]->getFd();
		s_listening.events = POLLIN;
		s_listening.revents = 0;
		_fds.push_back(s_listening);
	}
	while (true)
	{
		int ready = poll(_fds.data(), _fds.size(), pollTimeout());
		if (ready == -1)
		{
			if (errno == EINTR)
				continue;
			throw std::runtime_error("poll() fail");
		}
		for (size_t i = 0; i < _fds.size(); i++)
		{
			int		fd = _fds[i].fd;
			short	revents = _fds[i].revents;
			if (revents == 0 || fd == -1)
				continue;
			if (isMasterSocket(fd))
			{
				acceptClients(fd);
				continue;
			}
			if (_pipeToClient.count(fd))
			{
				handleCgiIo(fd, revents);
				continue;
			}
			if (_cgi.count(fd))
			{
				if (revents & (POLLHUP | POLLERR | POLLNVAL))
					abortCgi(fd);
				continue;
			}
			if ((revents & POLLOUT) && handleSend(fd) == false)
			{
				_clients.erase(fd);
				_fds.erase(_fds.begin() + i);
				i--;
			}
			else if ((revents & POLLIN) && handleClient(fd) == false)
			{
				_clients.erase(fd);
				_fds.erase(_fds.begin() + i);
				i--;
			}
		}
		checkCgiTimeouts();
		closeIdleConnections();
		compactPollFds();
	}
}

/*
 * Strips the optional port from a Host header, so "site.com:8080" and
 * "site.com" both match a server_name of "site.com". The result is lowercased
 * because the host of a request is case-insensitive, and server_name values are
 * stored lowercased for the same reason.
 */
std::string EventLoop::cleanHostHeader(const std::string &rawHost) const
{
	size_t	colon = rawHost.find(':');

	if (colon != std::string::npos)
		return (toLower(rawHost.substr(0, colon)));
	return (toLower(rawHost));
}

/*
 * Picks the server block that serves a request. Only blocks listening on the
 * port the request arrived on are considered; the one declaring the requested
 * Host as a server_name wins, and the first block on that port is the default
 * when no name matches.
 */
const ServerConfig &EventLoop::getServerConfigForRequest(int clientPort,
		const HttpRequest &request) const
{
	std::string			hostHeader = cleanHostHeader(
							request.getHeaderValue("Host"));
	const ServerConfig	*defaultServer = NULL;

	for (size_t i = 0; i < _configs.size(); ++i)
	{
		if (_configs[i].port != clientPort)
			continue;
		if (defaultServer == NULL)
			defaultServer = &_configs[i];
		for (size_t j = 0; j < _configs[i].serverNames.size(); ++j)
		{
			if (_configs[i].serverNames[j] == hostHeader)
				return (_configs[i]);
		}
	}
	if (defaultServer != NULL)
		return (*defaultServer);
	return (_configs[0]);
}

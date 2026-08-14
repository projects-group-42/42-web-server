/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   EventLoop.cpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dajesus- <dajesus-@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/04 19:05:52 by jucoelho          #+#    #+#             */
/*   Updated: 2026/08/13 02:03:20 by dajesus-         ###   ########.fr       */
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
#include <csignal>
#include <ctime>
#include <cstdlib>

static const long			CGI_TIMEOUT_MS = 5000;
static const double			IDLE_TIMEOUT_S = 30.0;
static const int			MAX_POLL_WAIT_MS = 1000;
static const int			BACKLOG = 128;

/*
 * Lowered by requestStop() when a signal asks the server to stop, and read by
 * run() between two turns of the loop. sig_atomic_t is the only type a handler
 * may touch safely.
 */
static volatile sig_atomic_t	g_running = 1;

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

/*
 * Releases everything the loop owns: the listening sockets and any CGI child
 * still running, which the CgiProcess destructor kills and reaps. The clients
 * close themselves through the Connection destructor.
 */
EventLoop::~EventLoop(void)
{
	for (std::map<int, CgiProcess*>::iterator it = _cgi.begin();
			it != _cgi.end(); ++it)
		delete it->second;
	_cgi.clear();
	for (size_t i = 0; i < _sckt.size(); i++)
		delete _sckt[i];
}

/**
 * @brief Installs the signal dispositions the server runs under.
 * SIGPIPE is ignored, so writing to a connection whose peer is already gone
 * fails with -1 out of send() instead of killing the process on the spot: a
 * client that closes abruptly is then just a write that made no progress, which
 * handleSend answers by dropping that one client while the server keeps
 * serving. A killed process is otherwise the default answer to that write, and
 * one client hanging up would take every other connection down with it.
 * SIGINT and SIGTERM are routed to requestStop so a stop is asked for rather
 * than taken, and the loop unwinds through its destructors.
 */
void EventLoop::setupSignals(void)
{
	signal(SIGPIPE, SIG_IGN);
	signal(SIGINT, EventLoop::requestStop);
	signal(SIGTERM, EventLoop::requestStop);
}

/*
 * Signal handler asking the loop to stop. It only lowers a flag: run() sees it
 * between two turns and returns, so the destructors do the freeing outside of
 * signal context.
 */
void EventLoop::requestStop(int signal)
{
	(void)signal;
	g_running = 0;
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

		Socket	*sckt = new Socket();

		/*
		 * One endpoint that cannot be bound (an interface this machine does not
		 * carry, a port already taken) is logged and skipped rather than taking
		 * the whole server down with it: the blocks that did bind still serve.
		 * run() still refuses to start when nothing bound at all.
		 */
		try
		{
			sckt->create();
			sckt->bind(host, port);
			sckt->listen(BACKLOG);
		}
		catch (const std::exception &e)
		{
			Logger::warning("Skipping " + host + ": " + e.what());
			delete sckt;
			continue;
		}
		_sckt.push_back(sckt);

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
 * what follows, because errno may not be consulted after a read: anything but a
 * positive count ends the connection, 0 being the peer closing and -1 an error
 * on a socket poll() had just reported as readable. Returning false makes run()
 * drop the client along with its poll entry.
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
 * Recognises the session the request belongs to, or opens one. The id travels
 * in the SESSIONID cookie the server sets; a request arriving without one, or
 * with an id this server does not know, starts a session of its own. The id is
 * kept on the connection so both the router and the CGI path can put it back
 * in the response.
 */
void EventLoop::openSession(Connection &conn)
{
	std::string	id = SessionStore::readCookie(
					conn.getRequest().getHeaderValue("Cookie"), "SESSIONID");

	_sessions.touch(id);
	conn.set_session_id(id);
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
	openSession(conn);

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
		response.setHeaders("set-cookie",
				_sessions.cookieFor(conn.get_session_id()));
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
 * As in handleClient, the return value of send() alone decides, because errno
 * may not be consulted after a write: a call that moved no byte (0) or failed
 * (-1) on a socket poll() had reported as writable ends the connection. send()
 * runs only while bytes are actually pending, so an empty buffer never produces
 * a 0 of its own.
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
			return (true);
	}
	if (!conn.get_keep_alive())
	{
		Logger::info("Response fully sent, closing connection.");
		return (false);
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
 * Drops a client connection: disarms its poll entry and erases it from
 * _clients, whose Connection destructor closes the socket fd. The single
 * place a client is ever removed, called from run() on a failed send/recv,
 * from closeIdleConnections() on a timeout, and from abortCgi() when a client
 * disconnects mid-CGI, so the fd and the Connection are always released
 * together and never by three slightly different sequences.
 */
void EventLoop::dropClient(int fd)
{
	disablePollFd(fd);
	_clients.erase(fd);
}

/*
 * Drops a CGI pipe fd: disarms its poll entry and forgets which client it
 * belonged to. The single place a pipe fd is ever removed, called from
 * handleCgiIo() when a direction finishes and from unregisterCgiPipes() when
 * a whole CGI is torn down early.
 */
void EventLoop::releasePipeFd(int fd)
{
	disablePollFd(fd);
	_pipeToClient.erase(fd);
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
 * once: a script executes only where the config binds an interpreter to its
 * extension, and any other request is served as a static file. There is no
 * default-interpreter fallback, so a location that never declares cgi_pass
 * cannot be made to execute an uploaded ".py" file, which is otherwise a path
 * to running attacker-supplied code out of an upload directory. A location
 * that redirects runs nothing, since the redirect is the answer and the CGI
 * would otherwise execute before the router ever sees the request. A method
 * the location does not accept runs nothing either, for the same reason: the
 * 405 is the answer, and it is the router that writes it.
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
	return (_router.resolveCgiInterpreter(uri, config));
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
	std::ostringstream			visits;

	/*
	 * The session the server keeps is handed to the script as well, so a CGI
	 * can greet a returning visitor on the very first request, before the
	 * browser has had a chance to send the cookie back.
	 */
	visits << _sessions.visits(conn.get_session_id());
	env.push_back("SESSION_ID=" + conn.get_session_id());
	env.push_back("SESSION_VISITS=" + visits.str());

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
 *
 * Every step pushes the deadline forward, so what CGI_TIMEOUT_MS bounds is how
 * long a child may go without moving a byte, not how long it may run. A child
 * spinning in a loop writes nothing and is still killed on time, while a body
 * of a hundred megabytes keeps the pipes busy and is allowed to finish.
 */
void EventLoop::handleCgiIo(int fd, short revents)
{
	int			clientFd = _pipeToClient[fd];
	CgiProcess	*proc = _cgi[clientFd];

	proc->setDeadlineMs(nowMs() + CGI_TIMEOUT_MS);

	if (fd == proc->bodyWriteFd())
	{
		if (revents & (POLLERR | POLLHUP | POLLNVAL))
			proc->stopWriting();
		else if (revents & POLLOUT)
			proc->onWritable();
		if (!proc->isWriting())
			releasePipeFd(fd);
	}
	else
	{
		if (revents & (POLLIN | POLLHUP | POLLERR))
			proc->onReadable();
		if (!proc->isReading())
			releasePipeFd(fd);
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
	{
		if (response.getHeaderValue("set-cookie").empty())
			response.setHeaders("set-cookie",
					_sessions.cookieFor(conn.get_session_id()));
		conn.set_write_buffer(builder.builder(conn.getRequest(), response));
	}
	else
		conn.set_write_buffer(buildError(conn, builder, 502));
	setPollEvents(clientFd, POLLOUT);
	releaseCgi(clientFd);
	Logger::info("CGI finished, response queued.");
}

/*
 * Removes every poll entry and mapping for the CGI pipes owned by clientFd.
 */
void EventLoop::unregisterCgiPipes(int clientFd)
{
	std::vector<int>	fds;

	for (std::map<int, int>::iterator it = _pipeToClient.begin(); it != _pipeToClient.end(); ++it)
	{
		if (it->second == clientFd)
			fds.push_back(it->first);
	}
	for (size_t i = 0; i < fds.size(); ++i)
		releasePipeFd(fds[i]);
}

/*
 * Releases everything a CGI in flight for clientFd owns: its pipe fds (through
 * unregisterCgiPipes, safe to call whether or not any are still registered)
 * and the CgiProcess itself, whose destructor kills and reaps the child if it
 * is still running. The single place a CGI is ever released, called when it
 * finishes normally, when its client disconnects mid-execution, and when it
 * times out, so the pipes and the child are always released together instead
 * of by three copies of the same three lines.
 */
void EventLoop::releaseCgi(int clientFd)
{
	CgiProcess	*proc = _cgi[clientFd];

	unregisterCgiPipes(clientFd);
	_cgi.erase(clientFd);
	delete proc;
}

/*
 * Tears down a CGI whose client disconnected mid-execution: releases the CGI
 * (pipes and child) and drops the client connection.
 */
void EventLoop::abortCgi(int clientFd)
{
	releaseCgi(clientFd);
	dropClient(clientFd);
	Logger::info("Client disconnected during CGI, process terminated.");
}

/*
 * Kills a CGI that ran past its deadline: releases the CGI (pipes and child,
 * reaped through the CgiProcess destructor), queues a 504 Gateway Timeout, and
 * arms the client for sending.
 */
void EventLoop::timeoutCgi(int clientFd)
{
	Connection		&conn = _clients[clientFd];
	ResponseBuilder	builder;

	releaseCgi(clientFd);
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
		dropClient(expired[i]);
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
 * Returns the timeout the main poll() waits with: the CGI deadline when one is
 * nearer, capped at MAX_POLL_WAIT_MS otherwise. The cap is what makes the loop
 * come back regularly with nothing to do, which is when the idle sweep runs
 * and when the stop flag is read. Waiting forever would leave a silent client
 * holding its descriptor until some other traffic woke the loop, and would
 * leave Ctrl+C unanswered for just as long.
 */
int EventLoop::pollTimeout(void)
{
	int	cgi = cgiPollTimeout();

	if (cgi == -1 || cgi > MAX_POLL_WAIT_MS)
		return (MAX_POLL_WAIT_MS);
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
	while (g_running)
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
				dropClient(fd);
			else if ((revents & POLLIN) && handleClient(fd) == false)
				dropClient(fd);
		}
		checkCgiTimeouts();
		closeIdleConnections();
		_sessions.sweep();
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

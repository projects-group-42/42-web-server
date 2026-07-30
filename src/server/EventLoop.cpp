/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   EventLoop.cpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/04 19:05:52 by jucoelho          #+#    #+#             */
/*   Updated: 2026/07/19 12:30:29 by jucoelho         ###   ########.fr       */
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
#include <cstdlib>

static const std::string	CGI_INTERPRETER = "/usr/bin/python3";

EventLoop::EventLoop(void) : _sckt(NULL), _router("www")
{
}

EventLoop::EventLoop(Socket *sckt) : _sckt(sckt), _router("www")
{
}

EventLoop::EventLoop(const EventLoop &copy)
	: _sckt(copy._sckt), _fds(copy._fds), _clients(copy._clients),
	  _router(copy._router), _cgiHandler(copy._cgiHandler),
	  _cgi(copy._cgi), _pipeToClient(copy._pipeToClient)
{
}

EventLoop::~EventLoop(void)
{
}

EventLoop &EventLoop::operator=(const EventLoop &other)
{
	if (this != &other)
	{
		_sckt = other._sckt;
		_fds = other._fds;
		_clients = other._clients;
		_router = other._router;
		_cgiHandler = other._cgiHandler;
		_cgi = other._cgi;
		_pipeToClient = other._pipeToClient;
	}
	return (*this);
}

void EventLoop::acceptClients(void)
{
	while (true)
	{
		int client = accept(_sckt->getFd(), NULL, NULL);
		if (client == -1)
			break;
		setNonBlocking(client);
		struct pollfd pfd;
		pfd.fd = client;
		pfd.events = POLLIN;
		pfd.revents = 0;
		_fds.push_back(pfd);
		_clients[client] = Connection(client);
		Logger::info("New client connected.");
	}
}

bool EventLoop::handleClient(int fd)
{
	ssize_t n = _clients[fd].receive_data();
	if (n > 0)
	{
		Logger::info("Data received from client.");
		if (_clients[fd].get_psr_state() == COMPLETE)
			handleRequest(fd);
		else if (_clients[fd].get_psr_state() == ERROR)
			handleParseError(fd);
		return true;
	}
	if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
		return true;
	return false;
}

void EventLoop::handleParseError(int fd)
{
	Connection		&conn = _clients[fd];
	ResponseBuilder	builder;

	int error_code = conn.get_error_code();
	if (error_code == 0)
		error_code = 400;

	std::string serialized = builder.buildErrorResponse(error_code);
	conn.set_write_buffer(serialized);

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
	Connection	&conn = _clients[fd];
	ResponseBuilder	builder;

	conn.set_keep_alive(wantsKeepAlive(conn.getRequest()));
	if (_cgiHandler.isCgiRequest(conn.getRequest().getUri()))
	{
		startCgi(fd);
		return ;
	}
	builder.setKeepAlive(conn.get_keep_alive());

	try
	{
		HttpResponse response;

		_router.route(conn.getRequest(), response);

		std::string serialized = builder.builder(conn.getRequest(), response);
		conn.set_write_buffer(serialized);
	}
	catch (std::exception &e)
	{
		Logger::error(std::string("Internal Server Error: ") + e.what());
		std::string serialized = builder.buildErrorResponse(500);
		conn.set_write_buffer(serialized);
	}
	catch (...)
	{
		Logger::error("Internal Server Error: unknown exception");
		std::string serialized = builder.buildErrorResponse(500);
		conn.set_write_buffer(serialized);
	}

	setPollEvents(fd, POLLOUT);
}

bool EventLoop::handleSend(int fd)
{
	Connection	&conn = _clients[fd];
	ssize_t		sent = conn.send_data();

	if (sent == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
		return true; // try again later
	if (sent == -1)
		return false; // error
	if (!conn.has_data_to_send())
	{
		if (!conn.get_keep_alive())
		{
			Logger::info("Response fully sent, closing connection.");
			return false; // done, close
		}
		conn.reset_for_next_request();
		setPollEvents(fd, POLLIN);
		Logger::info("Response fully sent, keeping connection alive.");
		if (conn.get_psr_state() == COMPLETE)
			handleRequest(fd);
		return true;
	}
	return true; // more to send
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
	conn.set_write_buffer(builder.buildErrorResponse(status));
	setPollEvents(fd, POLLOUT);
}

/*
 * Starts a CGI request without blocking the server: validates the script,
 * forks the interpreter, registers the CGI pipe fds in the poll set, and parks
 * the client fd (no interest) until the child finishes. On validation or fork
 * failure it queues the matching error response instead.
 */
void EventLoop::startCgi(int fd)
{
	Connection		&conn = _clients[fd];
	std::string		scriptPath;
	HttpResponse	errorResponse;

	if (!_cgiHandler.validate(conn.getRequest().getUri(), scriptPath, errorResponse))
	{
		sendCgiError(fd, errorResponse.getStatusCode());
		return ;
	}

	std::vector<std::string>	env = _cgiHandler.buildEnv(conn.getRequest(), scriptPath);
	CgiProcess					*proc = new CgiProcess(fd, conn.getRequest().getBody());

	if (!proc->start(CGI_INTERPRETER, scriptPath, env))
	{
		delete proc;
		sendCgiError(fd, 500);
		return ;
	}
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
 * Reaps the finished child, turns its collected output into an HTTP response
 * (502 when the script did not exit cleanly), arms the client for sending, and
 * releases the process.
 */
void EventLoop::finishCgi(int clientFd, CgiProcess *proc)
{
	Connection		&conn = _clients[clientFd];
	ResponseBuilder	builder;
	int				status = proc->reap();

	builder.setKeepAlive(conn.get_keep_alive());
	if (status == 0)
	{
		HttpResponse	response;

		response.setStatusCode(200);
		response.setBody(proc->output());
		conn.set_write_buffer(builder.builder(conn.getRequest(), response));
	}
	else
		conn.set_write_buffer(builder.buildErrorResponse(502));
	setPollEvents(clientFd, POLLOUT);
	_cgi.erase(clientFd);
	delete proc;
	Logger::info("CGI finished, response queued.");
}

/*
 * Tears down a CGI whose client disconnected mid-execution: unregisters its
 * pipe fds, kills and reaps the child, and drops the client connection.
 */
void EventLoop::abortCgi(int clientFd)
{
	CgiProcess	*proc = _cgi[clientFd];

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
	_cgi.erase(clientFd);
	delete proc;
	disablePollFd(clientFd);
	_clients.erase(clientFd);
	Logger::info("Client disconnected during CGI, process terminated.");
}

void EventLoop::run(void)
{
	if (!_sckt)
		throw std::runtime_error("EventLoop: no socket set");

	struct pollfd s_listening;
	s_listening.fd = _sckt->getFd();
	s_listening.events = POLLIN;
	s_listening.revents = 0;
	_fds.push_back(s_listening);

	while (true)
	{
		int ready = poll(_fds.data(), _fds.size(), -1);
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
			if (fd == _sckt->getFd())
			{
				acceptClients();
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
		compactPollFds();
	}
}

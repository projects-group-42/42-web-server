/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   EventLoop.cpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dajesus- <dajesus-@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/04 19:05:52 by jucoelho          #+#    #+#             */
/*   Updated: 2026/06/29 22:00:06 by dajesus-         ###   ########.fr       */
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

EventLoop::EventLoop(void) : _sckt(NULL), _router("www")
{
}

EventLoop::EventLoop(Socket *sckt) : _sckt(sckt), _router("www")
{
}

EventLoop::EventLoop(const EventLoop &copy)
	: _sckt(copy._sckt), _fds(copy._fds), _clients(copy._clients), _router(copy._router)
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
		// Check if we have a complete HTTP request (headers terminated)
		if (_clients[fd].get_psr_state() == COMPLETE)
			handleRequest(fd);
		return true;
	}
	if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
		return true;
	return false;
}

bool EventLoop::wantsKeepAlive(const HttpRequest &request) const
{
	std::string connection = toLower(request.getHeaderValue("connection"));
	if (connection == "close")
		return false;
	if (request.getVersion() == "HTTP/1.0")
		return (connection == "keep-alive");
	return true;
}

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
	HttpResponse		response;

	conn.set_keep_alive(wantsKeepAlive(conn.getRequest()));
	_router.route(conn.getRequest(), response);

	ResponseBuilder builder;
	builder.setKeepAlive(conn.get_keep_alive());
	std::string serialized = builder.builder(conn.getRequest(), response);
	conn.set_write_buffer(serialized);
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
			if (_fds[i].revents == 0)
				continue;
			if (_fds[i].fd == _sckt->getFd())
				acceptClients();
			else if ((_fds[i].revents & POLLOUT) && handleSend(_fds[i].fd) == false)
			{
				_clients.erase(_fds[i].fd);
				_fds.erase(_fds.begin() + i);
				i--;
			}
			else if ((_fds[i].revents & POLLIN) && handleClient(_fds[i].fd) == false)
			{
				_clients.erase(_fds[i].fd);
				_fds.erase(_fds.begin() + i);
				i--;
			}
		}
	}
}

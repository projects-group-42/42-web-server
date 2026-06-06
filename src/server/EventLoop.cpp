/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   EventLoop.cpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/04 19:05:52 by jucoelho          #+#    #+#             */
/*   Updated: 2026/06/06 19:11:45 by jucoelho         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "server/EventLoop.hpp"
#include "network/Connection.hpp"
#include "webserver.hpp"
#include <netinet/in.h>
#include <stdexcept>
#include <unistd.h>
#include <cerrno>
#include <fcntl.h>

EventLoop::EventLoop(void) : _sckt(NULL)
{
}

EventLoop::EventLoop(Socket *sckt): _sckt(sckt)
{
}

EventLoop::~EventLoop(void)
{
}

EventLoop::EventLoop(const EventLoop &copy): _sckt(copy._sckt),_pollfds(copy._pollfds)
{
}

EventLoop &EventLoop::operator=(const EventLoop &other)
{
	if (this != &other)
	{
		this->_sckt = other._sckt;
		this->_pollfds = other._pollfds;
	}
	return (*this);
}

/**
 * @brief Accepts a new incoming TCP connection and registers it.
 *
 * Called when poll() reports POLLIN on the listening socket fd.
 * Creates a Connection object for the new client, adds its fd
 * to _pollfds with POLLIN, and stores it in _map_client.
 *
 * @throws std::runtime_error if accept() fails.
 */
void EventLoop::acceptClient(void)
{
	struct sockaddr_in	c_addr;
	socklen_t			c_len;
	int					client_fd;
	struct pollfd		poll_fd;

	c_len = sizeof(c_addr);
	client_fd = accept(_pollfds[0].fd, (struct sockaddr *)&c_addr, &c_len);
	if (client_fd < 0)
		throw std::runtime_error("accept() fail");

	fcntl(client_fd, F_SETFL, O_NONBLOCK);
	poll_fd.fd = client_fd;
	poll_fd.events = POLLIN;
	poll_fd.revents = 0;
	_pollfds.push_back(poll_fd);

	Connection client(client_fd);
	_map_client[client_fd] = client;
	Logger::info("New client connected");
}
/**
 * @brief Starts the main asynchronous event loop (I/O Multiplexing).
 *
 * Registers the server's master listening socket into the poll structure,
 * then enters an infinite loop. It blocks until poll() detects activity.
 * Once triggered, it iterates through all registered file descriptors and
 * delegates tasks: accepting brand new clients on the master socket, or 
 * handing off existing client traffic to handleClientData().
 *
 * @throws std::runtime_error If poll() fails or if no server socket is bound.
 */
void EventLoop::handleClientData(int fd, int &i, int &total_sockets)
{
	int result = _map_client[fd].receive_data();
	
	if (result == 0)
	{
		Logger::warning("Client disconnected");
		_map_client.erase(fd);
		_pollfds.erase(_pollfds.begin() + i);
		total_sockets--;
		i--; // Ajusta os índices já que removemos um elemento do vetor
	}
	else if (result < 0)
	{
		// Se for EAGAIN ou EWOULDBLOCK, não faz nada e mantém a conexão viva
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			return;
			
		Logger::error("Fatal recv() fail");
		_map_client.erase(fd);
		_pollfds.erase(_pollfds.begin() + i);
		total_sockets--;
		i--;
	}
	else 
	{
		Logger::info("Data received! Current buffer: " + _map_client[fd]._read_buffer);
	}
}


void EventLoop::run(void)
{
	struct pollfd s_listening;

	if (!_sckt)
		throw std::runtime_error("EventLoop: no socket set");
		
	s_listening.fd = _sckt->getFd();
	s_listening.events = POLLIN;
	s_listening.revents = 0;
	_pollfds.push_back(s_listening);

	while (true)
	{
		int n_poll = poll(&_pollfds[0], _pollfds.size(), -1);
		if (n_poll < 0)
			throw std::runtime_error("poll() fail");

		int total_sockets = _pollfds.size();
		for (int i = 0; i < total_sockets; i++)
		{
			if (_pollfds[i].revents & POLLIN)
			{
				if (_pollfds[i].fd == _sckt->getFd())
					acceptClient();
				else
					handleClientData(_pollfds[i].fd, i, total_sockets);
			}
		}
	}
}

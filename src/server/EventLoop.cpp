/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   EventLoop.cpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/04 19:05:52 by jucoelho          #+#    #+#             */
/*   Updated: 2026/06/05 11:08:56 by jucoelho         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "server/EventLoop.hpp"
#include "utils/Logger.hpp"
#include <netinet/in.h>
#include <stdexcept>


EventLoop::EventLoop(void) : _sckt(NULL)
{
}

EventLoop::EventLoop(Socket *sckt): _sckt(sckt)
{
}

EventLoop::~EventLoop(void)
{
}

EventLoop::EventLoop(const EventLoop &copy): _sckt(copy._sckt), _fds(copy._fds)
{
}

EventLoop &EventLoop::operator=(const EventLoop &other)
{
	if (this != &other)
	{
		this->_sckt = other._sckt;
		this->_fds = other._fds;
	}
	return (*this);
}

void EventLoop::acceptClient(void)
{
	struct sockaddr_in	c_addr;
	socklen_t			c_len;
	int				client_fd;
	struct pollfd	poll_fd;

	c_len = sizeof(c_addr);
	client_fd = accept(_fds[0].fd, (struct sockaddr *)&c_addr, &c_len);
	if (client_fd < 0)
		throw std::runtime_error("accept() fail");

	poll_fd.fd = client_fd;
	poll_fd.events = POLLIN;
	poll_fd.revents = 0;
	_fds.push_back(poll_fd);
	Logger::info("New client connected");
}

void EventLoop::run(void)
{
	struct pollfd				s_listening;

	if (!_sckt)
		throw std::runtime_error("EventLoop: no socket set");
	s_listening.fd = _sckt->getFd();
	s_listening.events = POLLIN;
	s_listening.revents = 0;
	_fds.push_back(s_listening);

	while (true)
	{
		int n_poll = poll(&_fds[0], _fds.size(), -1);
		if(n_poll < 0)
		{
			throw std::runtime_error("poll() fail");
		}
		int new_fds = _fds.size();
		for (int i = 0; i < new_fds; i++)
		{
			if (_fds[i].revents & POLLIN)
			{
				if (_fds[i].fd == _sckt->getFd())
					acceptClient();
				else
					Logger::info("Future issue");
			}
		}
	}
}

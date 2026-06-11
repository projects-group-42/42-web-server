/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Socket.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/31 19:18:36 by jucoelho          #+#    #+#             */
/*   Updated: 2026/06/01 17:18:49 by jucoelho         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Socket.hpp"
#include "Utils.hpp"
#include "Logger.hpp"

#include <stdexcept>
#include <cstring>
#include <sstream>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

Socket::Socket(void): _fd(-1)
{
}

Socket::~Socket(void)
{
	if (_fd != -1)
		close(_fd);
}

void Socket::create(void)
{
	_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (_fd == -1)
		throw std::runtime_error("Error creating socket");
	int opt = 1;
	if	(setsockopt(_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))== -1)
	{
		close(_fd);
		_fd = -1;
		throw std::runtime_error("setsockopt(SO_REUSEADDR) fail");
	}
	FdUtils::setNonBlocking(_fd);
	Logger::info("Socket created with SO_REUSEADDR e O_NONBLOCK.");
}

void Socket::bind(const std::string &host, int port)
{
	struct	sockaddr_in addr;

	std::memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1)
	{
		close(_fd);
		_fd = -1;
		throw std::runtime_error("inet_pton() fail: invalid host: " + host);
	}
	if (::bind(_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
	{
		close(_fd);
		_fd = -1;
		std::ostringstream oss;
		oss << "bind() fail in port " << port;
		throw std::runtime_error(oss.str());
	}
	std::ostringstream oss;
	oss << "Socket bound in " << host << ":" << port;
	Logger::info(oss.str());
}

void Socket::listen(int backlog)
{
	if (::listen(_fd, backlog) == -1)
	{
		close(_fd);
		_fd = -1;
		throw std::runtime_error("listen() fail");
	}

	Logger::info("Socket in mode listen.");
}

int Socket::getFd(void) const
{
	return _fd;
}
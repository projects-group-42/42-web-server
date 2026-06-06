/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Socket.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/31 19:18:36 by jucoelho          #+#    #+#             */
/*   Updated: 2026/06/06 18:13:34 by jucoelho         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "network/Socket.hpp"
#include "webserver.hpp"

#include <stdexcept>
#include <cstring>
#include <sstream>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
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

Socket::Socket(const Socket &copy) : _fd(-1)
{
	*this = copy;
}

Socket &Socket::operator=(const Socket &other)
{
	if (this != &other)
		addr = other.addr;
	return (*this);
}

/**
 * @brief Creates a TCP socket and configures it for server use.
 *
 * Calls socket(), sets SO_REUSEADDR to avoid TIME_WAIT on restart,
 * and sets the fd to non-blocking mode with O_NONBLOCK.
 *
 * @throws std::runtime_error if socket(), setsockopt() or fcntl() fail.
 */
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
	setNonBlocking(_fd);
	Logger::info("Socket created with SO_REUSEADDR e O_NONBLOCK.");
}
/**
 * @brief Binds the socket to a host address and port.
 *
 * Converts the host string to a binary address with inet_pton()
 * and associates the socket with that (host:port) endpoint.
 *
 * @param host IPv4 address string (e.g. "127.0.0.1" or "0.0.0.0").
 * @param port Port number to listen on.
 * @throws std::runtime_error if inet_pton() or bind() fail.
 */
void Socket::bind(const std::string &host, int port)
{
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
/**
 * @brief Puts the socket in passive listening mode.
 *
 * After this call, the socket is ready to accept incoming TCP connections.
 * The backlog defines the maximum number of pending connections in the queue.
 *
 * @param backlog Maximum length of the pending connection queue.
 * @throws std::runtime_error if listen() fails.
 */
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
/**
 * @brief Returns the socket file descriptor.
 *
 * Used by the EventLoop to register this socket in poll()
 * and to detect incoming connections via POLLIN.
 *
 * @return The socket fd, or -1 if the socket has not been created yet.
 */
int Socket::getFd(void) const
{
	return _fd;
}

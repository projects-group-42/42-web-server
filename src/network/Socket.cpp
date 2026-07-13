/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Socket.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/31 19:18:36 by jucoelho          #+#    #+#             */
/*   Updated: 2026/07/11 16:02:32 by jucoelho         ###   ########.fr       */
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

/**
 * @brief Constructs a new Socket object.
 * * Initializes the file descriptor (_fd) to -1, indicating that 
 * the socket has not been created yet.
 */
Socket::Socket(void): _fd(-1)
{
}

/**
 * @brief Destroys the Socket object.
 * * Safely closes the socket file descriptor if it is currently open 
 * (i.e., not equal to -1), preventing file descriptor leaks.
 */
Socket::~Socket(void)
{
	if (_fd != -1)
		close(_fd);
}

/**
 * @brief Creates and configures the socket.
 * * This method creates an IPv4 (AF_INET), TCP (SOCK_STREAM) socket. 
 * It configures the socket to reuse local addresses (SO_REUSEADDR) to 
 * avoid "Address already in use" errors during quick restarts. 
 * It also sets the socket to non-blocking mode.
 * * @throw std::runtime_error If socket creation fails.
 * @throw std::runtime_error If setsockopt(SO_REUSEADDR) fails.
 */
void Socket::create(void)
{
	int opt = 1;

	_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (_fd == -1)
		throw std::runtime_error("Error creating socket");
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
 * @brief Binds the socket to a specific host IP and port.
 * * Converts the given host string (e.g., "127.0.0.1") into a network address 
 * and binds the created file descriptor to it. 
 * * @param host A string representing the IP address to bind to.
 * @param port An integer representing the port number.
 * * @throw std::runtime_error If the provided host IP is invalid (inet_pton fails).
 * @throw std::runtime_error If binding the socket to the port fails.
 */
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

/**
 * @brief Sets the socket to listen for incoming connections.
 * * @param backlog The maximum length to which the queue of pending 
 * connections for the socket may grow.
 * * @throw std::runtime_error If the listen() system call fails.
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
 * @brief Retrieves the socket's file descriptor.
 * * @return int The current file descriptor assigned to this socket.
 */
int Socket::getFd(void) const
{
	return _fd;
}

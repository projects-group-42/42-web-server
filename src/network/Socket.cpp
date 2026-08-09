/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Socket.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/31 19:18:36 by jucoelho          #+#    #+#             */
/*   Updated: 2026/06/04 20:08:15 by jucoelho         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "network/Socket.hpp"
#include "webserver.hpp"

#include <stdexcept>
#include <cstring>
#include <cstdlib>
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

/*
 * Converts a dotted-quad IPv4 literal into a network-order address.
 * inet_pton() is not one of the functions the subject authorises, and it is
 * not needed either: ConfigLoader already refuses anything that is not a
 * literal (or "localhost", which it maps to 127.0.0.1), so four decimal fields
 * are all there is to read. Returns false when the token is not such a literal.
 */
static bool parseIpv4(const std::string &host, struct in_addr &out)
{
	unsigned long	octet[4];
	std::string		field;
	size_t			start = 0;
	size_t			dot;

	for (int i = 0; i < 4; ++i)
	{
		dot = (i == 3) ? host.size() : host.find('.', start);
		if (dot == std::string::npos || dot == start)
			return (false);
		field = host.substr(start, dot - start);
		if (field.size() > 3
			|| field.find_first_not_of("0123456789") != std::string::npos)
			return (false);
		octet[i] = std::strtoul(field.c_str(), NULL, 10);
		if (octet[i] > 255)
			return (false);
		start = dot + 1;
	}
	out.s_addr = htonl(static_cast<unsigned int>(octet[0] << 24 | octet[1] << 16
					| octet[2] << 8 | octet[3]));
	return (true);
}

void Socket::bind(const std::string &host, int port)
{
	struct	sockaddr_in addr;

	std::memset(&addr, 0, sizeof(addr));

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	if (!parseIpv4(host, addr.sin_addr))
	{
		close(_fd);
		_fd = -1;
		throw std::runtime_error("invalid host: " + host);
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

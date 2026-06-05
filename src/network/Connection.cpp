/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Connection.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/05 12:21:14 by jucoelho          #+#    #+#             */
/*   Updated: 2026/06/05 13:02:01 by jucoelho         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "network/Connection.hpp"
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

Connection::Connection(void): _client_fd(-1)
{
}

Connection::Connection(int client_fd)
{
	_client_fd = client_fd;
	_time = clock();
}

Connection::~Connection(void)
{
	if (_client_fd != -1)
		close(_client_fd);
}

Connection::Connection(const Connection &copy): _client_fd(-1)
{
	*this = copy;
}

Connection &Connection::operator=(const Connection &other)
{
	if (this != &other)
	{
		_client_fd = other._client_fd;
		_read_buffer = other._read_buffer;
		_write_buffer = other._write_buffer;
		_time = other._time;
	}
	return (*this);
}

void	Connection::read_buffer(void)
{
	
}
void	Connection::write_buffer(void)
{

}

float Connection::last_activity(void)
{
	_time = clock() - _time;
	return ((float)_time/CLOCKS_PER_SEC);
}
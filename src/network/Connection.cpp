/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Connection.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/05 12:21:14 by jucoelho          #+#    #+#             */
/*   Updated: 2026/06/06 18:10:48 by jucoelho         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "network/Connection.hpp"

#include <sys/socket.h>

Connection::Connection(void) : _client_fd(-1), _time(time(NULL)), _psr_state(REQUEST_LINE)
{
}

Connection::Connection(int client_fd) : _client_fd(client_fd), _time(time(NULL)), _psr_state(REQUEST_LINE)
{
}

Connection::Connection(const Connection &copy)
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
		_psr_state = other._psr_state;
	}
	return (*this);
}

Connection::~Connection(void)
{
}

ssize_t Connection::receive_data(void)
{
	char	buffer[4096];
	ssize_t	bytes_read;

	bytes_read = recv(_client_fd, buffer, sizeof(buffer), 0);
	if (bytes_read > 0)
	{
		_read_buffer += std::string(buffer, bytes_read);
		_time = time(NULL);
	}
	return (bytes_read);
}

double Connection::last_activity(void) const
{
	return (difftime(time(NULL), _time));
}

t_psr_state Connection::get_psr_state(void) const
{
	return (_psr_state);
}

const std::string &Connection::get_read_buffer(void) const
{
	return (_read_buffer);
}

/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Connection.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dajesus- <dajesus-@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/05 12:21:14 by jucoelho          #+#    #+#             */
/*   Updated: 2026/06/29 21:17:08 by dajesus-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "network/Connection.hpp"
#include <sys/socket.h>
#include <unistd.h>

Connection::Connection(void) : _client_fd(-1), _time(time(NULL)), _parser()

{
}

Connection::Connection(int client_fd) : _client_fd(client_fd), _time(time(NULL)), _parser()
{
}

Connection::Connection(const Connection &copy)
	: _client_fd(copy._client_fd),
	  _write_buffer(copy._write_buffer),
	  _time(copy._time),
	  _parser(copy._parser)
{
	const_cast<Connection&>(copy)._client_fd = -1;
}

Connection &Connection::operator=(const Connection &other)
{
	if (this != &other)
	{
		if (_client_fd >= 0)
			close(_client_fd);
		_client_fd = other._client_fd;
		const_cast<Connection&>(other)._client_fd = -1;
		_write_buffer = other._write_buffer;
		_time = other._time;
		_parser = other._parser;
	}
	return (*this);
}

Connection::~Connection(void)
{
	if (_client_fd >= 0)
		close(_client_fd);
}

ssize_t Connection::receive_data(void)
{
	char	buffer[4096];
	ssize_t	bytes_read;

	bytes_read = recv(_client_fd, buffer, sizeof(buffer), 0);
	if (bytes_read > 0)
	{
		_parser.feed(buffer, bytes_read);
		_time = time(NULL);
	}
	return (bytes_read);
}

ssize_t Connection::send_data(void)
{
	ssize_t	sent = send(_client_fd, _write_buffer.data(),
						_write_buffer.size(), 0);
	if (sent > 0)
	{
		_write_buffer.erase(0, sent);
		_time = time(NULL);
	}
	return (sent);
}

bool	Connection::has_data_to_send(void) const
{
	return (!_write_buffer.empty());
}

void	Connection::set_write_buffer(const std::string &data)
{
	_write_buffer = data;
}

void	Connection::reset_write_buffer(void)
{
	_write_buffer.clear();
}

double Connection::last_activity(void) const
{
	return (difftime(time(NULL), _time));
}

t_psr_state Connection::get_psr_state(void) const
{
	return _parser.get_psr_state();
}

const HttpRequest& Connection::getRequest(void) const
{
	return _parser.getRequest();
}
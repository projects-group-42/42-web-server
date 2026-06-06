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
#include "webserver.hpp"

#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

Connection::Connection(void)
{
	_client_fd = -1;
	_cgi_pid = -1;
	_time = time(NULL);
	_psr_state = REQUEST_LINE;
}

Connection::Connection(int client_fd)
{
	_client_fd = client_fd;
	_cgi_pid = -1;
	_time = time(NULL);
	_psr_state = REQUEST_LINE;
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
		_cgi_pid = other._cgi_pid;
		_read_buffer = other._read_buffer;
		_write_buffer = other._write_buffer;
		_time = other._time;
		_psr_state = other._psr_state;
	}
	return (*this);
}

void	Connection::read_buffer(void)
{
	
}
void	Connection::write_buffer(void)
{

}

/**
 * @brief Returns the number of seconds since the last activity on this connection.
 *
 * Compares the current time against the timestamp of the last successful recv().
 * Used by the event loop to detect and close idle connections on timeout.
 *
 * @return Seconds elapsed since last activity.
 */
float Connection::last_activity(void) const
{
	int last_activity = time(NULL) - _time;
	return (last_activity);
}

/**
 * @brief Reads available data from the client socket into the read buffer.
 *
 * Called by the event loop when poll() reports POLLIN on this connection's fd.
 * Appends received bytes to _read_buffer and updates _time on success.
 *
 * @return Number of bytes read (> 0), 0 if client closed the connection,
 *         or -1 on error.
 */
int Connection::receive_data(void)
{
	char	buffer[4096];
	int		bytes_read;

	bytes_read = recv(_client_fd, buffer, sizeof(buffer), 0);
	if (bytes_read > 0)
	{
		_read_buffer += std::string(buffer, bytes_read);
		_time = time(NULL);
	}
	return (bytes_read);
}
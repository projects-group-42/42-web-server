/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Connection.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/05 12:21:14 by jucoelho          #+#    #+#             */
/*   Updated: 2026/08/10 00:00:00 by galves-a         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "network/Connection.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

Connection::Connection(void) : _client_fd(-1), _time(time(NULL)), _parser(), _keep_alive(false), _timed_out(false)
{
}

Connection::Connection(int client_fd) : _client_fd(client_fd), _time(time(NULL)), _parser(), _keep_alive(false), _timed_out(false)
{
}

/*
 * Builds a connection that remembers the address accept() reported for the
 * peer, which is what the CGI environment publishes as REMOTE_ADDR.
 * getpeername() is not one of the functions the subject authorises, so the
 * address is only ever read at accept time.
 */
Connection::Connection(int client_fd, const std::string &remote_addr)
	: _client_fd(client_fd), _remote_addr(remote_addr), _time(time(NULL)),
	  _parser(), _keep_alive(false), _timed_out(false)
{
}

Connection::Connection(const Connection &copy)
	: _client_fd(copy._client_fd),
	  _write_buffer(copy._write_buffer),
	  _remote_addr(copy._remote_addr),
	  _session_id(copy._session_id),
	  _time(copy._time),
	  _parser(copy._parser),
	  _keep_alive(copy._keep_alive),
	  _timed_out(copy._timed_out)
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
		_remote_addr = other._remote_addr;
		_session_id = other._session_id;
		_time = other._time;
		_parser = other._parser;
		_keep_alive = other._keep_alive;
		_timed_out = other._timed_out;
	}
	return (*this);
}

Connection::~Connection(void)
{
	if (_client_fd >= 0)
		close(_client_fd);
}

/*
 * Reads one ready chunk from the socket and feeds it to the parser. The result
 * of recv() is returned untouched, and errno is never consulted: the caller
 * (EventLoop::handleClient) decides on the value alone — a positive count is
 * data, 0 is the peer closing, -1 is an error, and both of the latter drop the
 * client.
 */
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

/*
 * Writes one ready chunk of the pending response and drops what left. As in
 * receive_data, the result of send() is returned untouched and errno is never
 * consulted: EventLoop::handleSend decides on the value alone, and anything
 * but a positive count drops the client.
 */
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

/*
 * Records whether this connection should stay open after the current
 * response, based on the client's request.
 */
void	Connection::set_keep_alive(bool keep_alive)
{
	_keep_alive = keep_alive;
}

/*
 * Caps how large a body the parser of this connection buffers before it
 * answers 413, so an oversized upload is refused while it is being read
 * instead of after it has been held whole in memory.
 */
void	Connection::setMaxBodySize(long maxBodySize)
{
	_parser.setMaxBodySize(maxBodySize);
}

/*
 * Returns whether this connection is to be reused after the response.
 */
bool	Connection::get_keep_alive(void) const
{
	return (_keep_alive);
}

/*
 * Rewinds the parser for the next request on a kept-alive connection
 * and refreshes the activity timestamp.
 */
void	Connection::reset_for_next_request(void)
{
	_parser.reset();
	_time = time(NULL);
	_timed_out = false;
}

double Connection::last_activity(void) const
{
	return (difftime(time(NULL), _time));
}

/*
 * Reports whether the peer has been silent for at least timeout seconds. Both
 * ends of the comparison come from time(), whose resolution is one second, so
 * a connection is dropped between timeout and timeout plus one second after
 * its last byte rather than exactly on the mark.
 */
bool Connection::is_idle(double timeout) const
{
	return (last_activity() >= timeout);
}

/*
 * Reports whether a request is halfway in. A parser that has moved past
 * REQUEST_LINE has already read part of one, and a parser still on
 * REQUEST_LINE holds buffered bytes only when the request line itself arrived
 * split. Neither is true of a connection that has said nothing since it was
 * accepted, or of one waiting between two requests, and a request that is
 * COMPLETE or in ERROR is answered by the normal path instead.
 */
bool Connection::has_partial_request(void) const
{
	t_psr_state	state = _parser.get_psr_state();

	if (state == COMPLETE || state == ERROR)
		return (false);
	return (state != REQUEST_LINE || _parser.hasBufferedData());
}

/*
 * Marks the connection as having already been answered for running out of
 * time. The timestamp is refreshed so the response that was just queued gets a
 * full window to leave, and the flag stops the sweep from queueing a second
 * one: a peer that does not read the first is closed on the next expiry.
 */
void Connection::mark_timed_out(void)
{
	_timed_out = true;
	_time = time(NULL);
}

bool Connection::timed_out(void) const
{
	return (_timed_out);
}

t_psr_state Connection::get_psr_state(void) const
{
	return _parser.get_psr_state();
}

int Connection::get_error_code(void) const
{
	return (_parser.get_error_code());
}

const HttpRequest& Connection::getRequest(void) const
{
	return _parser.getRequest();
}

/*
 * Returns the address of the peer as accept() reported it when the connection
 * was created, or an empty string for a connection built without one.
 */
const std::string &Connection::getRemoteAddr(void) const
{
	return (_remote_addr);
}

/*
 * Remembers the session the request being answered belongs to, so the response
 * can carry its cookie whichever path builds it.
 */
void	Connection::set_session_id(const std::string &id)
{
	_session_id = id;
}

const std::string &Connection::get_session_id(void) const
{
	return (_session_id);
}

/*
 * Returns the local port this connection was accepted on, so the request can
 * be matched against the server blocks listening on it. Returns 0 when the
 * socket cannot be queried.
 */
int Connection::getLocalPort(void) const
{
	struct sockaddr_in	address;
	socklen_t			len = sizeof(address);

	if (getsockname(_client_fd, (struct sockaddr *)&address, &len) == -1)
		return (0);
	return (ntohs(address.sin_port));
}

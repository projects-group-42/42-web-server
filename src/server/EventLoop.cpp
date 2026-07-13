/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   EventLoop.cpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/04 19:05:52 by jucoelho          #+#    #+#             */
/*   Updated: 2026/07/13 15:03:27 by jucoelho         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "server/EventLoop.hpp"
#include "http/ResponseBuilder.hpp"
#include "utils/Logger.hpp"
#include "utils/Utils.hpp"

#include <unistd.h>
#include <cerrno>
#include <stdexcept>
#include <sys/types.h>
#include <sys/socket.h>
#include <cstdlib>

/**
 * @brief Default constructor.
 *
 * Initializes socket pointer to NULL and router root to "www".
 * Use the parameterized constructor EventLoop(Socket *sckt) for normal usage.
 */
EventLoop::EventLoop(void) : _sckt(NULL), _router("www")
{
}
/**
 * @brief Parameterized constructor.
 *
 * Initializes the event loop with a listening socket and sets the default
 * document root for the router to "www".
 *
 * @param sckt Pointer to an initialized Socket object.
 */
EventLoop::EventLoop(Socket *sckt) : _sckt(sckt), _router("www")
{
}

/**
 * @brief Copy constructor for EventLoop.
 * * @param copy The EventLoop object to copy from.
 */
EventLoop::EventLoop(const EventLoop &copy)
	: _sckt(copy._sckt), _fds(copy._fds), _clients(copy._clients), _router(copy._router)
{
}

/**
 * @brief Destructor for EventLoop.
 */
EventLoop::~EventLoop(void)
{
}

/**
 * @brief Copy assignment operator for EventLoop.
 * * @param other The EventLoop object to assign from.
 * @return EventLoop& Reference to the updated object.
 */
EventLoop &EventLoop::operator=(const EventLoop &other)
{
	if (this != &other)
	{
		_sckt = other._sckt;
		_fds = other._fds;
		_clients = other._clients;
		_router = other._router;
	}
	return (*this);
}

/**
 * @brief Processes a fully parsed HTTP request and prepares the response.
 * * Routes the HTTP request to generate an appropriate HTTP response. Then, it 
 * uses the ResponseBuilder to serialize the response into a string, stores it 
 * in the client's write buffer, and changes the poll event to POLLOUT so the 
 * response can be sent asynchronously in the next loop iterations.
 * * @param fd The file descriptor of the client whose request is being handled.
 */
void EventLoop::handleRequest(int fd)
{
	Connection	&conn = _clients[fd];
	HttpResponse		response;

	_router.route(conn.getRequest(), response);

	ResponseBuilder builder;
	std::string serialized = builder.builder(conn.getRequest(), response);
	conn.set_write_buffer(serialized);

	// Switch this fd to POLLOUT so we can send the response
	for (size_t i = 0; i < _fds.size(); i++)
	{
		if (_fds[i].fd == fd)
		{
			_fds[i].events = POLLOUT;
			break;
		}
	}
}

/**
 * @brief Handles incoming data from a connected client.
 * * Reads data from the client's socket into the Connection's read buffer. 
 * If the HTTP request is fully received and parsed (state == COMPLETE), 
 * it triggers the request processing.
 * * @param fd The file descriptor of the client sending data.
 * @return true If the connection should be kept alive (data received successfully 
 * or operation would block - EAGAIN/EWOULDBLOCK).
 * @return false If the client disconnected (read 0 bytes) or a fatal error occurred.
 */
bool EventLoop::handleClient(int fd)
{
	ssize_t n = _clients[fd].receive_data();
	if (n > 0)
	{
		Logger::info("Data received from client.");
		// Check if we have a complete HTTP request (headers terminated)
		if (_clients[fd].get_psr_state() == COMPLETE)
			handleRequest(fd);
		return true;
	}
	if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
		return true;
	return false;
}

/**
 * @brief Handles sending the buffered HTTP response back to the client.
 * * Attempts to send chunks of the write buffer to the client. Handles partial 
 * sends and EAGAIN/EWOULDBLOCK states gracefully, ensuring the server doesn't 
 * block.
 * * @param fd The file descriptor of the client receiving the data.
 * @return true If there is still more data to send or the socket is temporarily 
 * blocking (EAGAIN/EWOULDBLOCK).
 * @return false If the entire response has been successfully sent or if a 
 * fatal error occurred (indicating the connection should be closed).
 */
bool EventLoop::handleSend(int fd)
{
	Connection	&conn = _clients[fd];
	ssize_t		sent = conn.send_data();

	if (sent == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
		return true; // try again later
	if (sent == -1)
		return false; // error
	if (!conn.has_data_to_send())
	{
		Logger::info("Response fully sent, closing connection.");
		return false; // done, close
	}
	return true; // more to send
}

/**
 * @brief Accepts new incoming client connections.
 * * Continuously accepts new connections on the listening socket until no more 
 * are pending. Sets each new client socket to non-blocking mode, registers 
 * it into the poll file descriptors vector (_fds) to monitor for POLLIN events, 
 * and initializes a Connection object for it.
 */
void EventLoop::acceptClients(void)
{
	while (true)
	{
		int client = accept(_sckt->getFd(), NULL, NULL);
		if (client == -1)
			break;
		setNonBlocking(client);
		struct pollfd pfd;
		pfd.fd = client;
		pfd.events = POLLIN;
		pfd.revents = 0;
		_fds.push_back(pfd);
		_clients[client] = Connection(client);
		Logger::info("New client connected.");
	}
}

/**
 * @brief Starts the main I/O multiplexing event loop.
 * * Initializes the pollfd structure with the main listening socket. Then, it 
 * enters an infinite loop using poll() to monitor all active file descriptors.
 * It dispatches events accordingly:
 * - If the listening socket has a POLLIN event, it accepts new clients.
 * - If a client socket has a POLLIN event, it reads data (handleClient).
 * - If a client socket has a POLLOUT event, it sends data (handleSend).
 * Automatically cleans up and removes file descriptors and Connection objects 
 * for clients that have disconnected or finished their transactions.
 * * @throw std::runtime_error If no listening socket is set or if poll() fails fatally.
 */
void EventLoop::run(void)
{
	if (!_sckt)
		throw std::runtime_error("EventLoop: no socket set");

	struct pollfd s_listening;
	s_listening.fd = _sckt->getFd();
	s_listening.events = POLLIN;
	s_listening.revents = 0;
	_fds.push_back(s_listening);

	while (true)
	{
		int ready = poll(_fds.data(), _fds.size(), -1);
		if (ready == -1)
		{
			if (errno == EINTR)
				continue;
			throw std::runtime_error("poll() fail");
		}
		for (size_t i = 0; i < _fds.size(); i++)
		{
			if (_fds[i].revents == 0)
				continue;
			if (_fds[i].fd == _sckt->getFd())
				acceptClients();
			else if ((_fds[i].revents & POLLOUT) && handleSend(_fds[i].fd) == false)
			{
				_clients.erase(_fds[i].fd);
				_fds.erase(_fds.begin() + i);
				i--;
			}
			else if ((_fds[i].revents & POLLIN) && handleClient(_fds[i].fd) == false)
			{
				_clients.erase(_fds[i].fd);
				_fds.erase(_fds.begin() + i);
				i--;
			}
		}
	}
}

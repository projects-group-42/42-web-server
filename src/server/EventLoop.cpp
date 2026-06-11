/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   EventLoop.cpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/04 19:05:52 by jucoelho          #+#    #+#             */
/*   Updated: 2026/06/04 20:06:38 by jucoelho         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <unistd.h>
#include <cerrno>
#include <stdexcept>
#include "webserver.hpp"
#include "server/EventLoop.hpp"

#include <sys/types.h>
#include <sys/socket.h>
#include <vector>
#include <poll.h>

static void accept_clients(Socket &sckt, std::vector<struct pollfd> &fds)
{
	while (true)
	{
		int client = accept(sckt.getFd(), NULL, NULL);
		if (client == -1)
			break;
		setNonBlocking(client);
		struct pollfd pfd;
		pfd.fd = client;
		pfd.events = POLLIN;
		pfd.revents = 0;
		fds.push_back(pfd);
		Logger::info("New client connected.");
	}
}

static bool handle_client(int fd)
{
	char buffer[4096];
	ssize_t n = recv(fd, buffer, sizeof(buffer), 0);
	if (n > 0)
		return true;
	if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
		return true;
	return false;
}

void event_loop(Socket &sckt)
{
	std::vector<struct pollfd> fds;
	struct pollfd s_listening;
	s_listening.fd = sckt.getFd();
	s_listening.events = POLLIN;
	s_listening.revents = 0;
	fds.push_back(s_listening);

	while (true)
	{
		int ready = poll(fds.data(), fds.size(), -1);
		if (ready == -1)
		{
			if (errno == EINTR)
				continue;
			throw std::runtime_error("poll() fail");
		}
		for (size_t i = 0; i < fds.size(); i++)
		{
			if (fds[i].revents == 0)
				continue;
			if (fds[i].fd == sckt.getFd())
				accept_clients(sckt, fds);
			else if (handle_client(fds[i].fd) == false)
			{
				close(fds[i].fd);
				fds.erase(fds.begin() + i);
				i--;
			}
		}
	}
}

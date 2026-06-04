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

#include <iostream>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "webserver.hpp"
#include "server/EventLoop.hpp"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <vector>
#include <poll.h>


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
		poll(fds.data(), fds.size(), -1);
		std::vector<struct pollfd>::iterator it;
		for (it = fds.begin(); it != fds.end(); it++)
		{
			if (it->revents != 0)
				Logger::info("Read to connect");	
		}
	}
}
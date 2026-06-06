/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   EventLoop.hpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/04 19:22:12 by jucoelho          #+#    #+#             */
/*   Updated: 2026/06/06 19:11:17 by jucoelho         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef 	EVENTLOOP_HPP
#define 	EVENTLOOP_HPP

#include "network/Socket.hpp"
#include "network/Connection.hpp"
#include <vector>
#include <poll.h>
#include <map>

class EventLoop
{
	private:
		Socket						*_sckt;
		std::vector<struct pollfd>	_pollfds;
		std::map<int, Connection >	_map_client;
		
	public:
		EventLoop(void);
		EventLoop(Socket *sckt);
		EventLoop(const EventLoop &copy);
		~EventLoop(void);

		EventLoop&		operator=(const EventLoop &other);
		
		void run(void);
		void acceptClient(void);
		void handleClientData(int fd, int &i, int &total_sockets);
};
#endif
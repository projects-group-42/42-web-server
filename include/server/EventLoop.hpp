/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   EventLoop.hpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/04 19:22:12 by jucoelho          #+#    #+#             */
/*   Updated: 2026/06/05 13:19:26 by jucoelho         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef 	EVENTLOOP_HPP
#define 	EVENTLOOP_HPP

#include "network/Socket.hpp"
#include <vector>
#include <poll.h>

class EventLoop
{
	private:
		Socket						*_sckt;
		std::vector<struct pollfd>	_fds;
		std::map<int, Connection >	_map_client;
		
	public:
		EventLoop(void);
		EventLoop(Socket *sckt);
		EventLoop(const EventLoop &copy);
		~EventLoop(void);

		EventLoop&		operator=(const EventLoop &other);
		
		void run(void);
		void acceptClient(void);
};
#endif
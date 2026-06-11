/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   EventLoop.hpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/04 19:22:12 by jucoelho          #+#    #+#             */
/*   Updated: 2026/06/04 20:03:56 by jucoelho         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef EVENTLOOP_HPP
#define EVENTLOOP_HPP

#include "network/Socket.hpp"
#include "network/Connection.hpp"
#include <vector>
#include <map>
#include <poll.h>

class EventLoop
{
	private:
		Socket						*_sckt;
		std::vector<struct pollfd>	_fds;
		std::map<int, Connection>	_clients;

		void	acceptClients(void);
		bool	handleClient(int fd);

	public:
		EventLoop(void);
		EventLoop(Socket *sckt);
		EventLoop(const EventLoop &copy);
		~EventLoop(void);

		EventLoop&	operator=(const EventLoop &other);

		void	run(void);
};
#endif

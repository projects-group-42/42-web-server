/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   EventLoop.hpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dajesus- <dajesus-@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/04 19:22:12 by jucoelho          #+#    #+#             */
/*   Updated: 2026/07/01 17:40:09 by dajesus-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef EVENTLOOP_HPP
#define EVENTLOOP_HPP

#include "network/Socket.hpp"
#include "network/Connection.hpp"
#include "http/Router.hpp"
#include "cgi/CgiHandler.hpp"
#include "cgi/CgiProcess.hpp"
#include <vector>
#include <map>
#include <poll.h>

class EventLoop
{
	private:
		Socket						*_sckt;
		std::vector<struct pollfd>	_fds;
		std::map<int, Connection>	_clients;
		Router						_router;
		CgiHandler					_cgiHandler;
		std::map<int, CgiProcess*>	_cgi;
		std::map<int, int>			_pipeToClient;

		void	acceptClients(void);
		bool	handleClient(int fd);
		void	handleParseError(int fd);
		void	handleRequest(int fd);
		bool	handleSend(int fd);
		bool	wantsKeepAlive(const HttpRequest &request) const;
		void	setPollEvents(int fd, short events);
		void	addPollFd(int fd, short events);
		void	disablePollFd(int fd);
		void	compactPollFds(void);
		void	startCgi(int fd);
		void	handleCgiIo(int fd, short revents);
		void	finishCgi(int clientFd, CgiProcess *proc);
		void	abortCgi(int clientFd);
		void	sendCgiError(int fd, int status);

	public:
		EventLoop(void);
		EventLoop(Socket *sckt);
		EventLoop(const EventLoop &copy);
		~EventLoop(void);

		EventLoop&	operator=(const EventLoop &other);

		void	run(void);
};
#endif

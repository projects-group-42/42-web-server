/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   EventLoop.hpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/04 19:22:12 by jucoelho          #+#    #+#             */
/*   Updated: 2026/08/01 22:45:30 by jucoelho         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef EVENTLOOP_HPP
#define EVENTLOOP_HPP

#include "network/Socket.hpp"
#include "network/Connection.hpp"
#include "http/Router.hpp"
#include "cgi/CgiHandler.hpp"
#include "cgi/CgiProcess.hpp"
#include "config/ServerConfig.hpp"
#include <vector>
#include <map>
#include <string>
#include <poll.h>

class EventLoop
{
	private:
		std::vector<Socket*>					_sckt;
		std::vector<int>						_boundPorts;
		std::vector<ServerConfig>				_configs;
		std::vector<struct pollfd>				_fds;
		std::map<int, Connection>				_clients;
		Router									_router;
		CgiHandler								_cgiHandler;
		std::map<int, CgiProcess*>				_cgi;
		std::map<int, int>						_pipeToClient;

		EventLoop(const EventLoop &copy);
		EventLoop&	operator=(const EventLoop &other);

		bool	isPortBound(int port) const;
		bool	isMasterSocket(int fd) const;
		void	acceptClients(int fd);
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
		void	unregisterCgiPipes(int clientFd);
		void	timeoutCgi(int clientFd);
		void	checkCgiTimeouts(void);
		int		cgiPollTimeout(void);
		void	sendCgiError(int fd, int status);
		std::string
				buildError(const Connection &conn,
				const ResponseBuilder &builder, int status) const;

	public:
		EventLoop(void);
		EventLoop(const std::vector<ServerConfig> &configs);
		~EventLoop(void);

		void	setupSockets(void);
		void	run(void);
		std::string
				cleanHostHeader(const std::string& rawHost) const;
		const ServerConfig&
				getServerConfigForRequest(int clientPort,
				const HttpRequest& request) const;
};
#endif

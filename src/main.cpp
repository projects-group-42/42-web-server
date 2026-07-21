/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/31 17:46:15 by jucoelho          #+#    #+#             */
/*   Updated: 2026/07/21 18:13:59 by jucoelho         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <fcntl.h>
#include <iostream>
#include <string>
#include <sstream>
#include <cstring>
#include <unistd.h>
#include "webserver.hpp"
#include "http/RequestParser.hpp"
#include "utils/Logger.hpp"
#include "http/RequestParser.hpp"
#include "http/HttpRequest.hpp"
#include "utils/Logger.hpp"
#include "network/Socket.hpp"
#include "server/EventLoop.hpp"

int main(void)
{
	const std::string	host = "0.0.0.0";
	const int			port = 8081;
	const int			backlog = 128;

	try
	{
		Socket sckt;

		sckt.create();
		sckt.bind(host, port);
		sckt.listen(backlog);
		int flags = fcntl(sckt.getFd(), F_GETFL, 0);
		if (flags != -1 && (flags & O_NONBLOCK))
			Logger::info("Socket is non-blocking.");
		else
			Logger::warning("Socket is blocking.");
		Logger::info("Listening on 0.0.0.0:8081 — connect with: nc localhost 8081");
		EventLoop loop(&sckt);
		loop.run();
	}
	catch (const std::exception &e)
	{
		Logger::error(e.what());
		return 1;
	}
	return 0;
}



 /*
int main(void)
{
	const std::string	host = "0.0.0.0";
	const int			port = 8081;
	const int			backlog = 128;

	try
	{
		Socket sckt;

		sckt.create();
		sckt.bind(host, port);
		sckt.listen(backlog);
		int flags = fcntl(sckt.getFd(), F_GETFL, 0);
		if (flags != -1 && (flags & O_NONBLOCK))
			Logger::info("Socket is non-blocking.");
		else
			Logger::warning("Socket is blocking.");
		Logger::info("Listening on 0.0.0.0:8080 — connect with: nc localhost 8080");
		EventLoop loop(&sckt);
		loop.run();
	}
	catch (const std::exception &e)
	{
		Logger::error(e.what());
		return 1;
	}
	return 0;
}*/

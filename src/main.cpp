/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/31 17:46:15 by jucoelho          #+#    #+#             */
/*   Updated: 2026/08/01 17:02:02 by jucoelho         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <unistd.h>
#include "utils/Logger.hpp"
#include "network/Socket.hpp"
#include "server/EventLoop.hpp"
#include "config/ConfigLoader1.hpp"
#include "config/Lexer.hpp"
#include "config/ServerConfig.hpp"

/* Maximum queue length specifiable by listen.  */
#define SOMAXCONN	4096

int main(int argc, char **argv)
{
	ConfigLoader config_file(argc == 2 ? argv[1]: "simple.conf");
	ServerConfig config = config_file.loader();
	
	signal(SIGPIPE, SIG_IGN);
	try
	{
		Socket sckt;

		sckt.create();
		sckt.bind(config.host, config.port);
		sckt.listen(SOMAXCONN);
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

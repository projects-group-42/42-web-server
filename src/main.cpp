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
#include <fcntl.h>
#include <sstream>
#include <string>
#include <unistd.h>
#include "utils/Logger.hpp"
#include "network/Socket.hpp"
#include "server/EventLoop.hpp"
#include "config/ConfigLoader.hpp"
#include "config/Lexer.hpp"
#include "config/ServerConfig.hpp"

#define BACKLOG	128

int main(int argc, char **argv)
{
	if (argc > 2)
	{
		Logger::error("usage: ./webserv [config_file]");
		return (1);
	}

	signal(SIGPIPE, SIG_IGN);
	try
	{
		ConfigLoader	config_file(argc == 2 ? argv[1] : "conf/simple.conf");
		ServerConfig	config = config_file.loader();
		Socket			sckt;

		sckt.create();
		sckt.bind(config.host, config.port);
		sckt.listen(BACKLOG);
		int flags = fcntl(sckt.getFd(), F_GETFL, 0);
		if (flags != -1 && (flags & O_NONBLOCK))
			Logger::info("Socket is non-blocking.");
		else
			Logger::warning("Socket is blocking.");

		std::ostringstream oss;
		oss << "Listening on " << config.host << ":" << config.port;
		Logger::info(oss.str());
		EventLoop loop(&sckt, config);
		loop.run();
	}
	catch (const std::exception &e)
	{
		Logger::error(e.what());
		return 1;
	}
	return 0;
}

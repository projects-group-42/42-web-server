/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/31 17:46:15 by jucoelho          #+#    #+#             */
/*   Updated: 2026/08/06 10:43:20 by jucoelho         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <csignal>
#include <fcntl.h>
#include <sstream>
#include <string>
#include <unistd.h>
#include <iostream>
#include "utils/Logger.hpp"
#include "network/Socket.hpp"
#include "server/EventLoop.hpp"
#include "config/ConfigLoader.hpp"
#include "config/Lexer.hpp"
#include "config/ServerConfig.hpp"

/* Maximum queue length specifiable by listen.  */
#define SOMAXCONN	4096

int main(int argc, char **argv)
{
	signal(SIGPIPE, SIG_IGN);
	try
	{
		ConfigLoader config_file(argc == 2 ? argv[1] : "conf/simple.conf");
		std::vector<ServerConfig> configs = config_file.loader();
		EventLoop loop(configs);
		loop.setupSockets();
		loop.run();
	}
	catch (const std::exception &e)
	{
		Logger::error(e.what());
		return 1;
	}
	return 0;
}

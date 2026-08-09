/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/31 17:46:15 by jucoelho          #+#    #+#             */
/*   Updated: 2026/08/01 22:21:59 by jucoelho         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <csignal>
#include <string>
#include <vector>
#include "utils/Logger.hpp"
#include "server/EventLoop.hpp"
#include "config/ConfigLoader.hpp"
#include "config/ServerConfig.hpp"

int main(int argc, char **argv)
{
	if (argc > 2)
	{
		Logger::error("usage: ./webserv [config_file]");
		return (1);
	}

	signal(SIGPIPE, SIG_IGN);
	signal(SIGINT, EventLoop::requestStop);
	signal(SIGTERM, EventLoop::requestStop);
	try
	{
		ConfigLoader				config_file(argc == 2
										? argv[1] : "conf/default.conf");
		std::vector<ServerConfig>	configs = config_file.loader();
		EventLoop					loop(configs);

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

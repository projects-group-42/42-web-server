/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/31 17:46:15 by jucoelho          #+#    #+#             */
/*   Updated: 2026/07/31 18:48:12 by jucoelho         ###   ########.fr       */
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
#include "config/ConfigUtils.hpp"
#include "config/Lexer.hpp"

int main(int argc, char **argv)
{
	HandleConfig config_file(argc == 2 ? argv[1]: "");
	ConfigBlock tree = config_file.handle();
	
	std::cout << "=== MEU CONFIGBLOCK ===" << std::endl;
	std::cout << "tree.name: " << tree.name << std::endl;
	std::cout << "tree.directives.size(): " << tree.directives.size() << std::endl;
	std::cout << "tree.children.size(): " << tree.children.size() << std::endl;
	for (size_t i = 0; i < tree.children.size(); i++)
	{
		std::cout << "  child[" << i << "].name: " << tree.children[i].name << std::endl;
		std::cout << "  child[" << i << "].directives.size(): " << tree.children[i].directives.size() << std::endl;
	}
	/*signal(SIGPIPE, SIG_IGN);
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
	}*/
	return 0;
}

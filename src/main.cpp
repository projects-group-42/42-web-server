/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/31 17:46:15 by jucoelho          #+#    #+#             */
/*   Updated: 2026/07/19 15:15:29 by jucoelho         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <fcntl.h>
#include "utils/Logger.hpp"
#include <string>
#include <cstring>
#include "utils/Logger.hpp"
#include "network/Socket.hpp"
#include "server/EventLoop.hpp"
#include <unistd.h>
#include <fcntl.h>
#include <string>
#include <cstring>
#include "utils/Logger.hpp"
#include "network/Socket.hpp"
#include "server/EventLoop.hpp"
#include <unistd.h>
#include <fcntl.h>

int main(int argc, char **argv)
{

    (void)argv;
    (void)argc;
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

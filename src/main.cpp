/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/31 17:46:15 by jucoelho          #+#    #+#             */
/*   Updated: 2026/06/01 17:18:32 by jucoelho         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <iostream>
#include <stdexcept>
#include <unistd.h>
#include "utils/Colors.hpp"
#include "utils/Logger.hpp"
#include "Socket.hpp"

int main(void)
{
	const std::string	host = "0.0.0.0";
	const int			port = 8080;
	const int			backlog = 128;

	std::cout << GREEN << "Webserv foundation is ready!" << RESET << std::endl;
	try
	{
		Socket server;

		server.create();
		server.bind(host, port);
		server.listen(backlog);
		while (true)
			pause();
	}
	catch (const std::exception &e)
	{
		Logger::error(e.what());
		return 1;
	}
	return 0;
}

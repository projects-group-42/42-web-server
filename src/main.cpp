/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/31 17:46:15 by jucoelho          #+#    #+#             */
/*   Updated: 2026/06/04 17:39:45 by jucoelho         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <iostream>
#include <unistd.h>
#include "Colors.hpp"
#include "Logger.hpp"
#include "Socket.hpp"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/fcntl.h>

int main(void)
{
	try
	{
		Socket s;
		s.create();
		s.bind("0.0.0.0", 8080);
		s.listen(128);

		int flags = fcntl(s.getFd(), F_GETFL, 0);
		if (flags & O_NONBLOCK)
			return (std::cout << "Socket is NON-BLOCKING", 0);
		else
			return (std::cout << "Socket is BLOCKING", 0);
		Logger::info("Listening on 0.0.0.0:8080 — connect with: nc localhost 8080");
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
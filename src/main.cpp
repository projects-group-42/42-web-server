/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/31 17:46:15 by jucoelho          #+#    #+#             */
/*   Updated: 2026/06/05 11:03:36 by jucoelho         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <iostream>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "webserver.hpp"
#include "network/Socket.hpp"
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <vector>
#include <poll.h>

int main(void)
{
	try
	{
		Socket sckt;
		sckt.create();
		sckt.bind("0.0.0.0", 8080);
		sckt.listen(128);
		Logger::info("Listening on 0.0.0.0:8080 — connect with: nc localhost 8080");
		EventLoop run(&sckt);
		run.run();
	}
	catch (const std::exception &e)
	{
		Logger::error(e.what());
		return 1;
	}
	return 0;
}
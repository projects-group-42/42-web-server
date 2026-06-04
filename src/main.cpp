/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/31 17:46:15 by jucoelho          #+#    #+#             */
/*   Updated: 2026/06/04 13:12:54 by jucoelho         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <iostream>
#include <unistd.h>
#include "Colors.hpp"
#include "Logger.hpp"
#include "Socket.hpp"

int main(void)
{
	try
	{
		Socket s;
		s.create();
		s.bind("0.0.0.0", 8080);
		s.listen(128);
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
/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Socket.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/31 19:18:36 by jucoelho          #+#    #+#             */
/*   Updated: 2026/05/31 19:24:39 by jucoelho         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Socket.hpp"
#include "Colors.hpp"

Socket::Socket(void): _fd(-1)
{
}

Socket::~Socket(void)
{
	if (fd != -1)
		close(fd);
}


void Socket::create(void)
{
	_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (_fd == -1)
		throw std::runtime_error("Erro ao criar socket");
	int opt = 1;
	setsockopt(_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));	
}

void Socket::bind(int port)
{
	_fd = port;
}

void Socket::listen(void)
{
	
}
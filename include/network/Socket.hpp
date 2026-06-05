/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Socket.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/31 19:15:00 by jucoelho          #+#    #+#             */
/*   Updated: 2026/06/05 10:59:41 by jucoelho         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef SOCKET_HPP
#define SOCKET_HPP

#include "utils/Logger.hpp"
#include <string>
#include <vector>
#include <poll.h>
#include <netinet/in.h>

class Socket
{
	private:
		struct	sockaddr_in addr;
		int		_fd;

		
	public:
		Socket();
		Socket(const Socket &copy);
		Socket& operator=(const Socket &other);
		~Socket();

		void	create(void);
		void	bind(const std::string &host, int port);
		void	listen(int backlog);
		int		getFd(void) const;
};
#endif
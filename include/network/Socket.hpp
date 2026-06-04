/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Socket.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/31 19:15:00 by jucoelho          #+#    #+#             */
/*   Updated: 2026/06/04 20:08:06 by jucoelho         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef SOCKET_HPP
#define SOCKET_HPP

#include "utils/Logger.hpp"
#include <string>
#include <vector>
#include <poll.h>

class Socket
{
	private:
		int	_fd;
		Socket(const Socket &copy);
		Socket& operator=(const Socket &other);
		std::vector<struct pollfd> fds;

	public:
		Socket();
		~Socket();

		void	create(void);
		void	bind(const std::string &host, int port);
		void	listen(int backlog);
		int		getFd(void) const;
};
#endif
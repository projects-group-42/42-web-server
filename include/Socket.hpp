/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Socket.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/31 19:15:00 by jucoelho          #+#    #+#             */
/*   Updated: 2026/06/01 17:01:26 by jucoelho         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef SOCKET_HPP
#define SOCKET_HPP

#include <string>

class Socket
{
	private:
		int	_fd;
		Socket(const Socket &copy);
		Socket& operator=(const Socket &other);

	public:
		Socket();
		~Socket();

		void	create(void);
		void	bind(const std::string &host, int port);
		void	listen(int backlog);
		int		getFd(void) const;
};
#endif
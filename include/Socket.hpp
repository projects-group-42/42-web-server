/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Socket.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/31 19:15:00 by jucoelho          #+#    #+#             */
/*   Updated: 2026/05/31 19:19:11 by jucoelho         ###   ########.fr       */
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
		void	bind(int port);
		void	listen(void);
};
#endif
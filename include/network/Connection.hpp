/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Connection.hpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/05 12:20:37 by jucoelho          #+#    #+#             */
/*   Updated: 2026/06/05 12:54:37 by jucoelho         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CONNECTION_HPP
#define CONNECTION_HPP

#include "utils/Logger.hpp"
#include <string>
#include <map>
#include <ctime>

class Connection
{
	private:
		int			_client_fd;
		std::string	_read_buffer;
		std::string	_write_buffer;
		clock_t		_time;
		
	public:
		Connection();
		Connection(int client_fd);
		Connection(const Connection &copy);
		Connection& operator=(const Connection &other);
		~Connection();

		void	read_buffer(void);
		void	write_buffer(void);
		float	last_activity(void);
};
#endif

/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Connection.hpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/05 12:20:37 by jucoelho          #+#    #+#             */
/*   Updated: 2026/06/21 13:35:12 by jucoelho         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CONNECTION_HPP
#define CONNECTION_HPP

#include <string>
#include <ctime>
#include <sys/types.h>
#include <http/RequestParser.hpp>

class Connection
{
	private:
		int				_client_fd;
		std::string		_read_buffer;
		std::string		_write_buffer;
		time_t			_time;
		RequestParser	_parser;

	public:
		Connection(void);
		Connection(int client_fd);
		Connection(const Connection &copy);
		Connection& operator=(const Connection &other);
		~Connection(void);

		ssize_t				receive_data(void);
		double				last_activity(void) const;
		const std::string&	get_read_buffer(void) const;

};
#endif

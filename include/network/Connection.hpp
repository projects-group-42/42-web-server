/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Connection.hpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dajesus- <dajesus-@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/05 12:20:37 by jucoelho          #+#    #+#             */
/*   Updated: 2026/06/29 21:16:29 by dajesus-         ###   ########.fr       */
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
		ssize_t				send_data(void);
		bool				has_data_to_send(void) const;
		void				set_write_buffer(const std::string &data);
		void				reset_write_buffer(void);
		double				last_activity(void) const;
		t_psr_state			get_psr_state(void) const;
		const HttpRequest&	getRequest(void) const;
};
#endif

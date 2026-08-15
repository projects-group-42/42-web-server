/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Connection.hpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/05 12:20:37 by jucoelho          #+#    #+#             */
/*   Updated: 2026/08/10 00:00:00 by galves-a         ###   ########.fr       */
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
		size_t			_write_sent;
		std::string		_remote_addr;
		std::string		_session_id;
		time_t			_time;
		RequestParser	_parser;
		bool			_keep_alive;
		bool			_timed_out;

	public:
		Connection(void);
		Connection(int client_fd);
		Connection(int client_fd, const std::string &remote_addr);
		Connection(const Connection &copy);
		Connection& operator=(const Connection &other);
		~Connection(void);

		ssize_t				receive_data(void);
		ssize_t				send_data(void);
		bool				has_data_to_send(void) const;
		void				set_write_buffer(const std::string &data);
		void				swap_write_buffer(std::string &data);
		void				swapRequestBody(std::string &body);
		void				reset_write_buffer(void);
		void				set_keep_alive(bool keep_alive);
		void				setMaxBodySize(long maxBodySize);
		bool				get_keep_alive(void) const;
		void				reset_for_next_request(void);
		double				last_activity(void) const;
		bool				is_idle(double timeout) const;
		bool				has_partial_request(void) const;
		void				mark_timed_out(void);
		bool				timed_out(void) const;
		t_psr_state			get_psr_state(void) const;
		int					get_error_code(void) const;
		const HttpRequest&	getRequest(void) const;
		int					getLocalPort(void) const;
		const std::string	&getRemoteAddr(void) const;
		void				set_session_id(const std::string &id);
		const std::string	&get_session_id(void) const;
};
#endif

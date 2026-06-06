/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Connection.hpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/05 12:20:37 by jucoelho          #+#    #+#             */
/*   Updated: 2026/06/06 18:37:42 by jucoelho         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CONNECTION_HPP
#define CONNECTION_HPP

#include "utils/Logger.hpp"
#include <string>
#include <ctime>

/**
 * @brief Enumeration of HTTP request parsing states.
 *
 * Tracks which phase of parsing the Connection is currently in.
 * Used to handle fragmented recv() calls correctly.
 *
 * REQUEST_LINE = Reading the first line (method, URI, version).
 *
 * HEADERS = Reading header fields until blank line (\r\n\r\n).
 *
 * BODY = Reading the request body (Content-Length or chunked).
 *
 * COMPLETE = Full request received and ready to be processed.
 */
typedef enum	e_psr_state
{
	REQUEST_LINE,
	HEADERS,
	BODY,
	COMPLETE
}				t_psr_state;

class Connection
{
	private:
		int			_client_fd;
		pid_t		_cgi_pid;
		//std::string	_read_buffer;
		std::string	_write_buffer;
		time_t		_time;
		t_psr_state	_psr_state;

	public:
		Connection();
		Connection(int client_fd);
		Connection(const Connection &copy);
		Connection& operator=(const Connection &other);
		~Connection();

		int			receive_data(void);
		float		last_activity(void) const;
		t_psr_state	get_psr_state();
		
		void	read_buffer(void);
		void	write_buffer(void);
		std::string	_read_buffer;
};
#endif

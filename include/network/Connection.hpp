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

#include <string>
#include <ctime>
#include <sys/types.h>

typedef enum e_psr_state
{
	REQUEST_LINE,
	HEADERS,
	BODY,
	COMPLETE
}	t_psr_state;

class Connection
{
	private:
		int			_client_fd;
		std::string	_read_buffer;
		std::string	_write_buffer;
		time_t		_time;
		t_psr_state	_psr_state;

	public:
		Connection(void);
		Connection(int client_fd);
		Connection(const Connection &copy);
		Connection& operator=(const Connection &other);
		~Connection(void);

		ssize_t				receive_data(void);
		double				last_activity(void) const;
		t_psr_state			get_psr_state(void) const;
		const std::string&	get_read_buffer(void) const;
};
#endif

/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   RequestParser.hpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/20 17:25:50 by jucoelho          #+#    #+#             */
/*   Updated: 2026/06/26 17:39:35 by jucoelho         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef REQUESTPARSER_HPP
#define REQUESTPARSER_HPP

#include <string>
#include <unistd.h>
#include <cstdlib>
# include "http/HttpRequest.hpp"

typedef enum e_psr_state
{
	REQUEST_LINE,
	HEADERS,
	BODY,
	COMPLETE,
	ERROR
}	t_psr_state;

class RequestParser
{
	private:
		std::string	_buffer;
		ssize_t		_len;
		t_psr_state	_psr_state;
		HttpRequest	_request;

	public:
		RequestParser(void);
		RequestParser(std::string buffer, ssize_t len);
		RequestParser(const RequestParser &copy);
		RequestParser& operator=(const RequestParser &other);
		~RequestParser(void);

		void feed(const char *buffer, ssize_t bytes_read);
		t_psr_state	get_psr_state(void) const;
		std::string str_extract(std::string str_find, int nbr);
		bool prs_method(void);
		bool prs_headers(void);
		bool prs_body(void);
		const HttpRequest& getRequest(void) const;
		std::string percent_decoding(std::string str);
};

#endif
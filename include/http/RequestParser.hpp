/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   RequestParser.hpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/20 17:25:50 by jucoelho          #+#    #+#             */
/*   Updated: 2026/07/17 17:06:37 by jucoelho         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef REQUESTPARSER_HPP
#define REQUESTPARSER_HPP

#include <string>
#include <unistd.h>
#include <cstdlib>
#include "http/HttpRequest.hpp"

#define MAX_URI_LENGTH 2048
#define MAX_HEADER_SIZE 8192

typedef enum e_psr_state
{
	REQUEST_LINE,
	HEADERS,
	BODY,
	CHUNK_SIZE,
	CHUNK_DATA,
	CHUNK_TRAILER,
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
		int			_error_code;
		int			_chunk_size;

		void				setErrorState(int status_code);
		bool				isValidVersion(const std::string &version) const;

	public:
		RequestParser(void);
		RequestParser(std::string buffer, ssize_t len);
		RequestParser(const RequestParser &copy);
		RequestParser& operator=(const RequestParser &other);
		~RequestParser(void);

		void feed(const char *buffer, ssize_t bytes_read);
		void reset(void);
		t_psr_state	get_psr_state(void) const;
		int			get_error_code(void) const;
		std::string str_extract(std::string str_find, int nbr);
		bool prs_method(void);
		bool prs_headers(void);
		bool prs_body(void);
		const HttpRequest& getRequest(void) const;
		std::string percent_decoding(std::string str);
		bool prs_chunked_body(void);
		bool RequestParser::prs_chunked_size(void);
		void RequestParser::prs_chunked_data(void);
};

#endif
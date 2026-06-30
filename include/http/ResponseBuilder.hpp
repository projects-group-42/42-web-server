/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ResponseBuilder.hpp                                :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dajesus- <dajesus-@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/12 13:37:03 by jucoelho          #+#    #+#             */
/*   Updated: 2026/06/29 21:21:07 by dajesus-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef RESPONSEBUILDER_HPP
# define RESPONSEBUILDER_HPP

# include "utils/Utils.hpp"
# include "http/HttpResponse.hpp"
# include "http/HttpRequest.hpp"

/*
 * ResponseBuilder
 *
 * Completes and serializes an HttpResponse into a raw HTTP/1.1 string
 * ready to be sent over a socket.
 *
 * The handler fills status, body and specific headers.
 * ResponseBuilder adds default headers (Date, Server, Content-Length)
 * and serializes everything.
 */
class ResponseBuilder
{
	private:
		std::string	_server_name;
		bool		_keep_alive;

		std::string getStatusMessage(int status) const;
		std::string getStatusLine(const HttpRequest 
			&request, const HttpResponse &response) const;
		void		setDefaultHeaders(HttpResponse &response) const;
		std::string	ErrorPage(int status) const;

	public:
		ResponseBuilder(void);
		ResponseBuilder(const std::string &serverName, bool keepAlive);
		ResponseBuilder& operator=(const ResponseBuilder &other);
		~ResponseBuilder(void);

		std::string	builder(const HttpRequest &request, HttpResponse &response)
	;
};

#endif

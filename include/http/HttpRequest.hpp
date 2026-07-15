/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpRequest.hpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/11 22:20:40 by dajesus-          #+#    #+#             */
/*   Updated: 2026/06/29 17:32:21 by jucoelho         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef HTTP_REQUEST_HPP
# define HTTP_REQUEST_HPP

# include <string>
# include <map>

class HttpRequest
{
	private:
		//request line
		std::string							_method;
		std::string							_uri; //request target
		std::string							_query; 
		std::string							_version;//protocol version
		//headers
		std::map<std::string, std::string>	_headers;
		//body
		std::string							_body;

	public:
		HttpRequest(void);
		HttpRequest(const HttpRequest &copy);
		HttpRequest& operator=(const HttpRequest &other);
		~HttpRequest(void);

		const std::string&	getMethod(void) const;
		const std::string&	getUri(void) const;
		const std::string&	getQuery(void) const;
		const std::string&	getVersion(void) const;
		const std::map<std::string,
			std::string>&	getHeaders(void) const;
		const std::string	getHeaderValue(const std::string &key) const;
		const std::string&	getBody(void) const;

		void				setMethod(const std::string &method);
		void				setUri(const std::string &uri);
		void				setQuery(const std::string &query);
		void				setVersion(const std::string &version);
		void				setHeaders(const std::string &key, const std::string &value);
		void				setBody(const std::string &body);

		bool				hasHeader(const std::string &key) const;
};

#endif

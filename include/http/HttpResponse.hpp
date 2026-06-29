/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpResponse.hpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/12 13:37:03 by jucoelho          #+#    #+#             */
/*   Updated: 2026/06/29 18:36:57 by jucoelho         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef HTTP_RESPONSE_HPP
# define HTTP_RESPONSE_HPP

# include <string>
# include <map>

class HttpResponse
{
	private:
		//status line
		std::string							_version;//protocol + version
		//status code
		int									_status_code;
		//headers
		std::map<std::string, std::string>	_headers;
		//body
		std::string							_body;

	public:
		HttpResponse(void);
		HttpResponse(const HttpResponse &copy);
		HttpResponse& operator=(const HttpResponse &other);
		~HttpResponse(void);

		const std::string&	getVersion(void) const;
		int			getStatusCode(void) const;
		const std::map<std::string,
			std::string>&	getHeaders(void) const;
		const std::string	getHeaderValue(const std::string &key) const;
		const std::string&	getBody(void) const;

		void				setVersion(const std::string &version);
		void				setStatusCode(int status);
		void				setHeaders(const std::string &key, const std::string &value);
		void				setBody(const std::string &body);
};

#endif

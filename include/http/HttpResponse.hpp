/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpResponse.hpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/12 13:37:03 by jucoelho          #+#    #+#             */
/*   Updated: 2026/06/26 17:44:23 by jucoelho         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef HTTP_RESPONSE_HPP
# define HTTP_RESPONSE_HPP

# include <string>
# include <map>

class HttpResponse
{
	private:
		std::string							_version;//protocol + version
		std::map<std::string, std::string>	_headers;
		std::string							_body;
		int									_status;

	public:
		HttpResponse(void);
		HttpResponse(const HttpResponse &copy);
		HttpResponse& operator=(const HttpResponse &other);
		~HttpResponse(void);

		const std::string&	getVersion(void) const;
		const std::map<std::string, std::string>& getHeaders(void) const;
		const std::string&	getBody(void) const;
		int					getStatus(void) const;

		void	setVersion(const std::string &version);
		void	addHeader(const std::string &key, const std::string &value);
		void	setBody(const std::string &body);
		void	setStatus(int status);		
		void	setDefaultHeaders(void);
		
		std::string whatTimeIsIt(void);
		std::string getHeader(const std::string &key) const;
		std::string getStatusMessage(void) const;
		std::string responseBuilder(void) const;
		std::string getStatusLine(void) const;

};

#endif

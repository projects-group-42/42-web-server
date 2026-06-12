/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpRequest.hpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dajesus- <dajesus-@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/11 22:20:40 by dajesus-          #+#    #+#             */
/*   Updated: 2026/06/11 22:43:47 by dajesus-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef HTTP_REQUEST_HPP
# define HTTP_REQUEST_HPP

# include <string>

class HttpRequest
{
	private:
		std::string							_method;
		std::string							_uri;
		std::string							_query;
		std::string							_version;
		std::string							_headers;
		std::string							_body;

	public:
		HttpRequest(void);
		HttpRequest(const HttpRequest &copy);
		HttpRequest& operator=(const HttpRequest &other);
		~HttpRequest(void);
};

#endif

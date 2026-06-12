/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpRequest.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dajesus- <dajesus-@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/11 22:21:02 by dajesus-          #+#    #+#             */
/*   Updated: 2026/06/11 22:43:40 by dajesus-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

# include "http/HttpRequest.hpp"

HttpRequest::HttpRequest(void)
{
}

HttpRequest::HttpRequest(const HttpRequest &copy)
{
	*this = copy;
}

HttpRequest& HttpRequest::operator=(const HttpRequest &other)
{
	if (this != &other)
	{
		_method = other._method;
		_uri = other._uri;
		_query = other._query;
		_version = other._version;
		_headers = other._headers;
		_body = other._body;
	}
	return (*this);
}

HttpRequest::~HttpRequest(void)
{
}


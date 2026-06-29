/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpResponse.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/12 13:36:33 by jucoelho          #+#    #+#             */
/*   Updated: 2026/06/29 18:37:10 by jucoelho         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

# include "http/HttpResponse.hpp"
# include "utils/Utils.hpp"
# include <iostream>
# include <sstream>
# include <ctime>

HttpResponse::HttpResponse(void)
	: _version("HTTP/1.1"), _status_code(200)
{
}

HttpResponse::HttpResponse(const HttpResponse &copy)
{
	*this = copy;
}

HttpResponse& HttpResponse::operator=(const HttpResponse &other)
{
	if (this != &other)
	{
		_version = other._version;
		_headers = other._headers;
		_body = other._body;
		_status_code = other._status_code;
	}
	return (*this);
}

HttpResponse::~HttpResponse(void)
{
}

const std::string& HttpResponse::getVersion(void) const
{
	return (_version);
}

int HttpResponse::getStatusCode(void) const
{
	return (_status_code);
}

const std::map<std::string, std::string>& HttpResponse::getHeaders(void) const
{
	return (_headers);
}

const std::string HttpResponse::getHeaderValue(const std::string &key) const
{
	std::map<std::string, std::string>::const_iterator it = _headers.find(toLower(key));
	if (it != _headers.end())
		return (it->second);
	return ("");
}

const std::string& HttpResponse::getBody(void) const
{
	return (_body);
}


void	HttpResponse::setVersion(const std::string &version)
{
	_version = version;
}

void	HttpResponse::setStatusCode(int status)
{
	_status_code = status;
}
void	HttpResponse::setHeaders(const std::string &key, const std::string &value)
{
	_headers[toLower(key)] = value;
}

void	HttpResponse::setBody(const std::string &body)
{
	_body = body;
}

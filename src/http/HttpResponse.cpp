/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpResponse.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dajesus- <dajesus-@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/12 13:36:33 by jucoelho          #+#    #+#             */
/*   Updated: 2026/06/29 22:25:46 by dajesus-         ###   ########.fr       */
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

const std::vector<std::pair<std::string, std::string> >& HttpResponse::getHeaders(void) const
{
	return (_headers);
}

const std::string HttpResponse::getHeaderValue(const std::string &key) const
{
	std::string	lowered = toLower(key);

	for (size_t i = 0; i < _headers.size(); ++i)
	{
		if (_headers[i].first == lowered)
			return (_headers[i].second);
	}
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
	std::string	lowered = toLower(key);

	for (size_t i = 0; i < _headers.size(); ++i)
	{
		if (_headers[i].first == lowered)
		{
			_headers[i].second = value;
			return ;
		}
	}
	_headers.push_back(std::make_pair(lowered, value));
}

void	HttpResponse::addHeader(const std::string &key, const std::string &value)
{
	_headers.push_back(std::make_pair(toLower(key), value));
}

void	HttpResponse::setBody(const std::string &body)
{
	_body = body;
}

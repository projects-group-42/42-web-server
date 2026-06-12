/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpRequest.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/11 22:21:02 by dajesus-          #+#    #+#             */
/*   Updated: 2026/06/12 14:40:56 by jucoelho         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

# include "http/HttpRequest.hpp"
# include "utils/Utils.hpp"

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

const std::string& HttpRequest::getMethod(void) const
{
	return (_method);
}

const std::string& HttpRequest::getUri(void) const
{
	return (_uri);
}

const std::string& HttpRequest::getQuery(void) const
{
	return (_query);
}

const std::string& HttpRequest::getVersion(void) const
{
	return (_version);
}

const std::map<std::string, std::string>& HttpRequest::getHeaders(void) const
{
	return (_headers);
}

const std::string& HttpRequest::getBody(void) const
{
	return (_body);
}

void	HttpRequest::setMethod(const std::string &method)
{
	_method = method;
}

void	HttpRequest::setUri(const std::string &uri)
{
	_uri = uri;
}

void	HttpRequest::setQuery(const std::string &query)
{
	_query = query;
}

void	HttpRequest::setVersion(const std::string &version)
{
	_version = version;
}

void	HttpRequest::addHeader(const std::string &key, const std::string &value)
{
	_headers[toLower(key)] = value;
}

void	HttpRequest::setBody(const std::string &body)
{
	_body = body;
}

bool	HttpRequest::hasHeader(const std::string &key) const
{
	return (_headers.find(toLower(key)) != _headers.end());
}

std::string HttpRequest::getHeader(const std::string &key) const
{
	std::map<std::string, std::string>::const_iterator it = _headers.find(toLower(key));
	if (it != _headers.end())
		return (it->second);
	return ("");
}

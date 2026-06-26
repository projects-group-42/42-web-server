/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Router.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dajesus- <dajesus-@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/24 20:47:41 by dajesus-          #+#    #+#             */
/*   Updated: 2026/06/25 21:01:30 by dajesus-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "http/Router.hpp"
#include "http/HttpResponse.hpp"

Router::Router(void)
	: _handler("www")
{
}

Router::Router(const std::string &root)
	: _handler(root)
{
}

Router::Router(const Router &copy)
	: _handler(copy._handler)
{
}

Router &Router::operator=(const Router &other)
{
	if (this != &other)
		_handler = other._handler;
	return (*this);
}

Router::~Router(void)
{
}

void	Router::setRoot(const std::string &root)
{
	_handler.setRoot(root);
}

void	Router::setIndex(const std::string &index)
{
	_handler.setIndex(index);
}

const std::string &Router::getRoot(void) const
{
	return (_handler.getRoot());
}

bool	Router::parseRequestLine(const std::string &buffer,
				std::string &method, std::string &uriPath) const
{
	std::string::size_type	lineEnd = buffer.find("\r\n");
	if (lineEnd == std::string::npos)
		return (false);

	std::string	line = buffer.substr(0, lineEnd);

	std::string::size_type	firstSpace = line.find(' ');
	if (firstSpace == std::string::npos)
		return (false);

	method = line.substr(0, firstSpace);

	std::string::size_type	secondSpace = line.rfind(' ');
	if (secondSpace == std::string::npos || secondSpace <= firstSpace)
		return (false);

	uriPath = line.substr(firstSpace + 1, secondSpace - firstSpace - 1);
	return (true);
}

bool	Router::route(const std::string &requestBuffer,
				std::string &response)
{
	if (requestBuffer.find("\r\n\r\n") == std::string::npos)
		return (false);

	std::string	method;
	std::string	uriPath;
	if (!parseRequestLine(requestBuffer, method, uriPath))
	{
		response = _handler.buildResponse(400, "text/html",
					_handler.buildErrorBody(400, ""), false);
		return (true);
	}

	if (method == "GET")
		return (_handler.handleGet(requestBuffer, response));

	response = _handler.buildResponse(501, "text/html",
				_handler.buildErrorBody(501, ""), false);
	return (true);
}

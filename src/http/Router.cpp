/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Router.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/24 20:47:41 by dajesus-          #+#    #+#             */
/*   Updated: 2026/06/26 18:36:23 by jucoelho         ###   ########.fr       */
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

bool	Router::route(const HttpRequest &request,
				std::string &response)
{
	if (request.getMethod() == "GET")
		return (_handler.handleGet(request, response));

	response = _handler.buildResponse(501, "text/html",
				_handler.buildErrorBody(501, ""), false);
	return (true);
}

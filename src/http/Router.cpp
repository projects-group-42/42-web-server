/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Router.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dajesus- <dajesus-@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/24 20:47:41 by dajesus-          #+#    #+#             */
/*   Updated: 2026/06/29 21:56:45 by dajesus-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "http/Router.hpp"
#include "http/HttpResponse.hpp"
#include "http/ResponseBuilder.hpp"
#include "utils/Logger.hpp"

Router::Router(void)
	: _staticHandler("www"), _responseBuilder("Webserv/1.0", false)
{
	_handlers["GET:/"] = &_staticHandler;
	_handlers["POST:/"] = &_staticHandler;
	_handlers["DELETE:/"] = &_staticHandler;
}

Router::Router(const std::string &root)
	: _staticHandler(root), _responseBuilder("Webserv/1.0", false)
{
	_handlers["GET:/"] = &_staticHandler;
	_handlers["POST:/"] = &_staticHandler;
	_handlers["DELETE:/"] = &_staticHandler;
}

Router::Router(const Router &copy)
	: _staticHandler(copy._staticHandler), _handlers(copy._handlers), _responseBuilder(copy._responseBuilder)
{
	_handlers["GET:/"] = &_staticHandler;
	_handlers["POST:/"] = &_staticHandler;
	_handlers["DELETE:/"] = &_staticHandler;
}

Router &Router::operator=(const Router &other)
{
	if (this != &other)
	{
		_staticHandler = other._staticHandler;
		_handlers = other._handlers;
		_responseBuilder = other._responseBuilder;
		_handlers["GET:/"] = &_staticHandler;
		_handlers["POST:/"] = &_staticHandler;
		_handlers["DELETE:/"] = &_staticHandler;
	}
	return (*this);
}

Router::~Router(void)
{
}

static std::string makeKey(const std::string &method, const std::string &path)
{
	return (method + ":" + path);
}

void	Router::addHandler(const std::string &method,
			const std::string &path, IRequestHandler *handler)
{
	_handlers[makeKey(method, path)] = handler;
}

void	Router::setRoot(const std::string &root)
{
	_staticHandler.setRoot(root);
}

void	Router::setIndex(const std::string &index)
{
	_staticHandler.setIndex(index);
}

const std::string &Router::getRoot(void) const
{
	return (_staticHandler.getRoot());
}

IRequestHandler *Router::resolveHandler(const std::string &method,
		const std::string &uri, bool &pathFound, std::string &allow)
{
	std::string bestKey;
	std::string bestPath;
	std::map<std::string, bool> methods;

	pathFound = false;

	for (std::map<std::string, IRequestHandler*>::iterator it =
			_handlers.begin(); it != _handlers.end(); ++it)
	{
		const std::string &key = it->first;

		size_t colonPos = key.find(':');
		if (colonPos == std::string::npos)
			continue;

		std::string keyMethod = key.substr(0, colonPos);
		std::string keyPath = key.substr(colonPos + 1);

		if (uri.compare(0, keyPath.size(), keyPath) == 0)
		{
			pathFound = true;
			methods[keyMethod] = true;
			if (keyMethod == method && keyPath.size() > bestPath.size())
			{
				bestKey = key;
				bestPath = keyPath;
			}
		}
	}

	allow.clear();
	for (std::map<std::string, bool>::const_iterator it = methods.begin();
			it != methods.end(); ++it)
	{
		if (!allow.empty())
			allow.append(", ");
		allow.append(it->first);
	}

	if (!bestKey.empty())
		return (_handlers[bestKey]);
	return (NULL);
}

bool	Router::route(const HttpRequest &request,
				HttpResponse &response)
{
	bool pathFound = false;
	std::string allow;
	IRequestHandler *handler = resolveHandler(
			request.getMethod(), request.getUri(), pathFound, allow);

	if (handler == NULL)
	{
		if (pathFound)
		{
			response.setStatusCode(405);
			response.setHeaders("Allow", allow);
		}
		else
			response.setStatusCode(501);
		response.setBody("");
		Logger::warning("No handler for: " + request.getMethod() + " "
				+ request.getUri());
	}
	else
	{
		handler->handle(request, response);
	}
	return (true);
}

/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Router.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/24 20:47:41 by dajesus-          #+#    #+#             */
/*   Updated: 2026/08/06 12:54:00 by jucoelho         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "http/Router.hpp"
#include "http/HttpResponse.hpp"
#include "http/ResponseBuilder.hpp"
#include "utils/Logger.hpp"
#include "http/MimeType.hpp"
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <iostream>

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

static std::string makeErrorPagePath(const std::string &root,
						 const std::string &errorPath)
{
	if (errorPath.empty())
		return ("");
	if (root.empty())
	{
		if (!errorPath.empty() && errorPath[0] == '/')
			return (errorPath.substr(1));
		return (errorPath);
	}

	std::string fullPath = root;
	if (!fullPath.empty() && fullPath[fullPath.size() - 1] == '/' &&
		!errorPath.empty() && errorPath[0] == '/')
		fullPath.append(errorPath.begin() + 1, errorPath.end());
	else if (!fullPath.empty() && fullPath[fullPath.size() - 1] != '/' &&
		!errorPath.empty() && errorPath[0] != '/')
	{
		fullPath.push_back('/');
		fullPath.append(errorPath);
	}
	else
		fullPath.append(errorPath);
	return (fullPath);
}

static bool readFileBody(const std::string &filePath, std::string &body)
{
	int fd = open(filePath.c_str(), O_RDONLY);
	if (fd == -1)
		return (false);

	char buffer[4096];
	size_t bytes;
	while ((bytes = read(fd, buffer, sizeof(buffer))) > 0)
		body.append(buffer, bytes);

	close(fd);
	return (bytes != static_cast<size_t>(-1));
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

void Router::serveCustomErrorPage(HttpResponse &response,
								  const ServerConfig &config,
								  const std::string &finalRoot)
{
	if (response.getStatusCode() < 400)
		return;
	std::map<int, std::string>::const_iterator it;
	it = config.errorPages.find(response.getStatusCode());
	if (it == config.errorPages.end())
		return;
	std::string errorPath = makeErrorPagePath(finalRoot, it->second);
	std::string body;
	if (!readFileBody(errorPath, body))
	{
		Logger::warning("Unable to load error page file: " + errorPath);
		return;
	}
	Logger::info("Serving custom error page: " + errorPath);
	response.setBody(body);
	std::string contentType = mimeType_resolve(errorPath);
	if (!contentType.empty())
		response.setHeaders("content-type", contentType);
}

bool Router::checkAllowedMethods(const HttpRequest &request,
								 HttpResponse &response,
								 const LocationConfig &location)
{
	std::string allow;

	if (location.allowedMethods.empty())
		return (true);
	for (size_t i = 0; i < location.allowedMethods.size(); ++i)
	{
		if (location.allowedMethods[i] == request.getMethod())
			return (true);
	}
	for (size_t i = 0; i < location.allowedMethods.size(); ++i)
	{
		if (i > 0)
			allow += ", ";

		allow += location.allowedMethods[i];
	}
	response.setStatusCode(405);
	response.setHeaders("Allow", allow);
	response.setBody("");
	return (false);
}

int Router::resolveLocation(const HttpRequest &request,
							 const ServerConfig &config, 
							 std::string &finalRoot,
							 std::string &finalIndex)
{
	std::string bestMatchPath;
	int bestMatchIndex = -1;

	for (size_t i = 0; i < config.locations.size(); ++i)
	{
		const std::string &locPath = config.locations[i].path;

		if (request.getUri().compare(0, locPath.size(), locPath) == 0)
		{
			Logger::info("Location encontrada: " + config.locations[i].path);
			Logger::info("Root da location: " + config.locations[i].root);
			Logger::info("Index configurado: " + config.locations[i].index);
			if (locPath.size() > bestMatchPath.size())
			{
				bestMatchPath = locPath;
				bestMatchIndex = i;
				if (!config.locations[i].root.empty())
					finalRoot = config.locations[i].root;
				if (!config.locations[i].index.empty())
					finalIndex = config.locations[i].index;
			}
		}
	}
	return (bestMatchIndex);
}


bool	Router::route(const HttpRequest &request,
				HttpResponse &response, const ServerConfig &config)
{
	bool pathFound = false;
	std::string allow;
	IRequestHandler *handler = resolveHandler(
			request.getMethod(), request.getUri(), pathFound, allow);
	std::string finalRoot = config.root;
	std::string finalIndex = config.index;
	std::string bestMatchPath = "";
	//loop por todas as locations do servidor

	int bestMatchIndex = resolveLocation(request, config, finalRoot, finalIndex);
	if (bestMatchIndex != -1 && (
			!checkAllowedMethods(request, response,
			config.locations[bestMatchIndex])))
		return (true);
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
		Logger::info("FINAL ROOT: " + finalRoot);
		Logger::info("FINAL INDEX: " + finalIndex);
		setRoot(finalRoot);
		setIndex(finalIndex);
		handler->handle(request, response);
	}
	serveCustomErrorPage(response, config, finalRoot);
	return (true);
}

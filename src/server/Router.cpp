/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Router.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/24 20:47:41 by dajesus-          #+#    #+#             */
/*   Updated: 2026/08/06 11:27:37 by jucoelho         ###   ########.fr       */
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
	int			bestMatchIndex = -1;
	//loop por todas as locations do servidor
	for (size_t i = 0; i < config.locations.size(); ++i)
	{
		const std::string &locPath = config.locations[i].path;
		// Verifica se o URI da requisição começa com este path
		if (request.getUri().compare(0, locPath.size(), locPath) == 0)
		{
			Logger::info("Location encontrada: " + config.locations[i].path);
			Logger::info("Root da location: " + config.locations[i].root);
			Logger::info("Index configurado: " + config.locations[i].index);
			// Se é a melhor match até agora (maior/mais específico path)
			if (locPath.size() > bestMatchPath.size())
			{
				bestMatchPath = locPath;
				bestMatchIndex = i;
				// Usa root e index da location se tiverem configurados
				if (!config.locations[i].root.empty())
				{
					finalRoot = config.locations[i].root;
				}
				if (!config.locations[i].index.empty())
				{
					finalIndex = config.locations[i].index;
				}
			}
		}
	}
	if (bestMatchIndex != -1)
	{
		if (!config.locations[bestMatchIndex].allowedMethods.empty())
		{
			
			bool methodAllowed = false;
			for (size_t i = 0; i < config.locations[bestMatchIndex].allowedMethods.size(); ++i)
			{
				if (config.locations[bestMatchIndex].allowedMethods[i] == request.getMethod())
				{
					methodAllowed = true;
					break;
				}
			}
			if (!methodAllowed)
			{
				response.setStatusCode(405);
				std::string result;
				for (size_t i = 0; i < config.locations[bestMatchIndex].allowedMethods.size(); ++i)
				{
					if (i > 0)
						result += ", ";
					result += config.locations[bestMatchIndex].allowedMethods[i];
				}
				response.setHeaders("Allow", result);
				response.setBody("");
				return (true);
			}
		}
	}
// Se não encontrou handler, retorna erro apropriado
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
// Handler encontrado, processa a requisição
	else
	{
		Logger::info("FINAL ROOT: " + finalRoot);
		Logger::info("FINAL INDEX: " + finalIndex);
		setRoot(finalRoot);
		setIndex(finalIndex);
		handler->handle(request, response);
	}
// Se houve erro, tenta servir página de erro customizada
	if (response.getStatusCode() >= 400)
	{
		std::map<int, std::string>::const_iterator error_it;
		error_it = config.errorPages.find(response.getStatusCode());
		if (error_it != config.errorPages.end())
		{
			std::string errorPath = makeErrorPagePath(finalRoot,
								error_it->second);
			std::string body;
			if (readFileBody(errorPath, body))
			{
				Logger::info("Serving custom error page: " + errorPath);
				response.setBody(body);
				std::string contentType = mimeType_resolve(errorPath);
				if (!contentType.empty())
					response.setHeaders("content-type", contentType);
			}
			else
			{
				Logger::warning("Unable to load error page file: " + errorPath);
			}
		}
	}
	return (true);
}

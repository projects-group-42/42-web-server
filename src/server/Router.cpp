/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Router.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/24 20:47:41 by dajesus-          #+#    #+#             */
/*   Updated: 2026/08/02 01:44:21 by jucoelho         ###   ########.fr       */
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
	struct stat	info;

	if (stat(filePath.c_str(), &info) == -1 || !S_ISREG(info.st_mode))
		return (false);

	int fd = open(filePath.c_str(), O_RDONLY);
	if (fd == -1)
		return (false);

	char	buffer[4096];
	ssize_t	bytes;
	while ((bytes = read(fd, buffer, sizeof(buffer))) > 0)
		body.append(buffer, static_cast<size_t>(bytes));

	close(fd);
	if (bytes == -1)
	{
		body.clear();
		return (false);
	}
	return (true);
}

/*
 * Loads the custom error page configured for a status code.
 *
 * Resolves the configured path against the given root, reads the file and
 * returns its contents plus the matching MIME type. Returns false when no
 * page is configured for the status or when the file cannot be read, so the
 * caller keeps the built-in default body.
 */
bool	Router::loadErrorPage(const ServerConfig &config, const std::string &root,
			int status, std::string &body, std::string &contentType)
{
	std::map<int, std::string>::const_iterator it = config.errorPages.find(status);

	if (it == config.errorPages.end())
		return (false);

	std::string errorPath = makeErrorPagePath(root, it->second);
	body.clear();
	if (!readFileBody(errorPath, body))
	{
		Logger::warning("Unable to load error page file: " + errorPath);
		return (false);
	}
	contentType = mimeType_resolve(errorPath);
	return (true);
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
	for (size_t i = 0; i < config.locations.size(); ++i)
	{
		const std::string &locPath = config.locations[i].path;
		if (request.getUri().compare(0, locPath.size(), locPath) == 0)
		{
			if (locPath.size() > bestMatchPath.size())
			{
				bestMatchPath = locPath;
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
		setRoot(finalRoot);
		setIndex(finalIndex);
		handler->handle(request, response);
	}

	if (response.getStatusCode() >= 400)
	{
		std::string body;
		std::string contentType;
		if (loadErrorPage(config, finalRoot, response.getStatusCode(),
				body, contentType))
		{
			response.setBody(body);
			if (!contentType.empty())
				response.setHeaders("content-type", contentType);
		}
	}
	return (true);
}

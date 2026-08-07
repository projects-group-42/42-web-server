/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Router.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: galves-a <galves-a@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/24 20:47:41 by dajesus-          #+#    #+#             */
/*   Updated: 2026/08/07 01:29:29 by galves-a         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "http/Router.hpp"
#include "http/HttpResponse.hpp"
#include "http/MimeType.hpp"
#include "http/ResponseBuilder.hpp"
#include "utils/Logger.hpp"
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

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

/**
 * @brief Finds the location block that applies to a URI in a server block.
 * The location whose path is the longest matching prefix of the URI wins, the
 * same literal prefix rule nginx uses, so "/test" also matches "/testing.html".
 * @param uri The request target.
 * @param config The server block serving the request.
 * @return The winning location, or NULL when no location matches.
 */
const LocationConfig	*Router::matchLocation(const std::string &uri,
			const ServerConfig &config) const
{
	const LocationConfig	*best = NULL;

	for (size_t i = 0; i < config.locations.size(); ++i)
	{
		const std::string	&locPath = config.locations[i].path;

		if (uri.compare(0, locPath.size(), locPath) != 0)
			continue;
		if (best == NULL || locPath.size() > best->path.size())
			best = &config.locations[i];
	}
	return (best);
}

/**
 * @brief Extracts the extension of the last segment of a URI.
 * A dot belonging to a parent directory is ignored, so "/cgi.d/script" is not
 * mistaken for a ".d/script" extension.
 * @param uri The request target.
 * @return The extension including its leading dot, or an empty string when the
 * last segment carries none.
 */
static std::string	uriExtension(const std::string &uri)
{
	std::string::size_type	dot = uri.rfind('.');
	std::string::size_type	slash = uri.rfind('/');

	if (dot == std::string::npos)
		return ("");
	if (slash != std::string::npos && dot < slash)
		return ("");
	return (uri.substr(dot));
}

/**
 * @brief Picks the interpreter that runs the CGI script a URI points to.
 * The interpreter is looked up by script extension in the cgi_pass directives
 * of the location matching the URI, so "/cgi/app.py" is run by the interpreter
 * bound to ".py" there.
 * @param uri The request target.
 * @param config The server block serving the request.
 * @return The interpreter path, or an empty string when the matching location
 * binds no interpreter to the extension of the URI.
 */
std::string	Router::resolveCgiInterpreter(const std::string &uri,
			const ServerConfig &config) const
{
	const LocationConfig	*best = matchLocation(uri, config);
	std::string				extension = uriExtension(uri);

	if (best == NULL || extension.empty())
		return ("");

	std::map<std::string, std::string>::const_iterator	it
		= best->cgiPass.find(extension);

	if (it == best->cgiPass.end())
		return ("");
	return (it->second);
}

/**
 * @brief Picks the document root that applies to a URI in a server block.
 * When the matching location declares no root of its own it inherits the
 * server root.
 * @param uri The request target.
 * @param config The server block serving the request.
 * @return The document root to serve the request from.
 */
std::string	Router::resolveRoot(const std::string &uri,
			const ServerConfig &config) const
{
	const LocationConfig	*best = matchLocation(uri, config);

	if (best != NULL && !best->root.empty())
		return (best->root);
	return (config.root);
}

/**
 * @brief Picks the index file that applies to a URI in a server block.
 * ConfigLoader already copies the server index into every location that
 * declares none, so an empty location index only happens when the server
 * declares none either, and the server value is used as the fallback.
 * @param uri The request target.
 * @param config The server block serving the request.
 * @return The index file name to serve directories with.
 */
std::string	Router::resolveIndex(const std::string &uri,
			const ServerConfig &config) const
{
	const LocationConfig	*best = matchLocation(uri, config);

	if (best != NULL && !best->index.empty())
		return (best->index);
	return (config.index);
}

/**
 * @brief Tells whether a generated directory listing applies to a URI.
 * ConfigLoader already copies the server flag into every location that
 * declares none, so the server value is only reached when the URI matches no
 * location at all.
 * @param uri The request target.
 * @param config The server block serving the request.
 * @return true when a directory without an index should be listed.
 */
bool	Router::resolveAutoindex(const std::string &uri,
			const ServerConfig &config) const
{
	const LocationConfig	*best = matchLocation(uri, config);

	if (best != NULL)
		return (best->autoindex);
	return (config.autoindex);
}

/**
 * @brief Picks the maximum request body size that applies to a URI.
 * A location declaring no size of its own keeps the sentinel -1 and inherits
 * the server value, so a location only overrides it when it declares one.
 * @param uri The request target.
 * @param config The server block serving the request.
 * @return The maximum body size in bytes, or -1 when no limit applies.
 */
long	Router::resolveMaxBodySize(const std::string &uri,
			const ServerConfig &config) const
{
	const LocationConfig	*best = matchLocation(uri, config);

	if (best != NULL && best->clientMaxBodySize >= 0)
		return (best->clientMaxBodySize);
	return (config.clientMaxBodySize);
}

/**
 * @brief Joins a document root and a configured error page path.
 * The page path is written root-relative in the config file, so exactly one
 * separator is kept between the two halves.
 * @param root The document root the page is resolved against.
 * @param page The path declared by the error_page directive.
 * @return The path of the error page on disk.
 */
static std::string	joinErrorPagePath(const std::string &root,
			const std::string &page)
{
	std::string	path = root;

	if (path.empty())
		return (page[0] == '/' ? page.substr(1) : page);
	if (path[path.size() - 1] == '/')
		path.erase(path.size() - 1);
	if (page[0] != '/')
		path.push_back('/');
	path.append(page);
	return (path);
}

/**
 * @brief Reads a regular file into a string.
 * Directories and special files are refused so a misconfigured error page
 * never turns into an unreadable body.
 * @param path The file to read.
 * @param body The destination holding the file contents.
 * @return true when the whole file could be read.
 */
static bool	readErrorPageFile(const std::string &path, std::string &body)
{
	struct stat	info;
	char		buffer[4096];
	ssize_t		bytes;

	if (stat(path.c_str(), &info) == -1 || !S_ISREG(info.st_mode))
		return (false);

	int	fd = open(path.c_str(), O_RDONLY);

	if (fd == -1)
		return (false);
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

/**
 * @brief Loads the error page a server block configured for a status code.
 * Resolves the configured path against the given root, reads the file and
 * reports the MIME type that matches it. Returns false when the status has no
 * page or the file cannot be read, so the caller keeps the built-in body
 * instead of answering an empty one.
 * @param config The server block serving the request.
 * @param root The document root the page is resolved against.
 * @param status The status code being answered.
 * @param body The destination holding the page contents.
 * @param contentType The destination holding the MIME type of the page.
 * @return true when a configured page was read.
 */
bool	Router::loadErrorPage(const ServerConfig &config,
			const std::string &root, int status, std::string &body,
			std::string &contentType)
{
	std::map<int, std::string>::const_iterator	it
		= config.errorPages.find(status);

	if (it == config.errorPages.end() || it->second.empty())
		return (false);

	std::string	path = joinErrorPagePath(root, it->second);

	body.clear();
	if (!readErrorPageFile(path, body))
	{
		Logger::warning("error_page: cannot read '" + path + "'");
		return (false);
	}
	contentType = mimeType_resolve(path);
	return (true);
}

/**
 * @brief Tells whether a URI is answered by a redirect.
 * CGI is dispatched before the router runs, so the caller driving it has to
 * know a location redirects to keep a script from executing instead.
 * @param uri The request target.
 * @param config The server block serving the request.
 * @return true when the location matching the URI declares a return.
 */
bool	Router::redirects(const std::string &uri,
			const ServerConfig &config) const
{
	const LocationConfig	*best = matchLocation(uri, config);

	return (best != NULL && best->returnCode != 0);
}

/**
 * @brief Answers a request matching a location that declares a return.
 * The redirect is emitted before any handler runs, so the location needs no
 * root of its own and the target is never looked up on disk. Every method is
 * redirected, the same rewrite-before-content order nginx applies.
 * @param request The request being answered.
 * @param response The response to fill in place.
 * @param config The server block serving the request.
 * @return true when the request was answered with a redirect.
 */
bool	Router::applyRedirect(const HttpRequest &request,
			HttpResponse &response, const ServerConfig &config) const
{
	const LocationConfig	*best = matchLocation(request.getUri(), config);

	if (best == NULL || best->returnCode == 0)
		return (false);
	response.setStatusCode(best->returnCode);
	response.setHeaders("Location", best->returnUrl);
	response.setBody("");
	Logger::info("Redirecting " + request.getUri() + " to "
			+ best->returnUrl);
	return (true);
}

bool	Router::route(const HttpRequest &request,
				HttpResponse &response, const ServerConfig &config)
{
	bool pathFound = false;
	std::string allow;

	if (applyRedirect(request, response, config))
		return (true);

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
		std::string	root = resolveRoot(request.getUri(), config);
		std::string	index = resolveIndex(request.getUri(), config);

		if (!root.empty())
			setRoot(root);
		if (!index.empty())
			setIndex(index);
		_staticHandler.setAutoindex(
				resolveAutoindex(request.getUri(), config));
		_staticHandler.setMaxBodySize(
				resolveMaxBodySize(request.getUri(), config));
		handler->handle(request, response);
	}
	applyErrorPage(request, response, config);
	return (true);
}

/**
 * @brief Replaces the body of an error response with its configured page.
 * The page is resolved against the root of the location that matched the URI,
 * so a location with its own root keeps serving its own error pages. Responses
 * below 400 and statuses without a configured page are left untouched.
 * @param request The request being answered.
 * @param response The response to rewrite in place.
 * @param config The server block serving the request.
 */
void	Router::applyErrorPage(const HttpRequest &request,
			HttpResponse &response, const ServerConfig &config) const
{
	std::string	body;
	std::string	contentType;

	if (response.getStatusCode() < 400)
		return ;
	if (!loadErrorPage(config, resolveRoot(request.getUri(), config),
			response.getStatusCode(), body, contentType))
		return ;
	response.setBody(body);
	if (!contentType.empty())
		response.setHeaders("content-type", contentType);
}

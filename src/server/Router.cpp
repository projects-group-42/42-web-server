/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Router.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/24 20:47:41 by dajesus-          #+#    #+#             */
/*   Updated: 2026/08/08 18:08:20 by jucoelho         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "http/Router.hpp"
#include "http/HttpResponse.hpp"
#include "http/MimeType.hpp"
#include "http/ResponseBuilder.hpp"
#include "utils/Logger.hpp"
#include <sys/stat.h>
#include <fstream>

Router::Router(void)
	: _staticHandler("www"), _responseBuilder("Webserv/1.0", false)
{
	_handlers["GET:/"] = &_staticHandler;
	_handlers["HEAD:/"] = &_staticHandler;
	_handlers["POST:/"] = &_staticHandler;
	_handlers["DELETE:/"] = &_staticHandler;
}

Router::Router(const std::string &root)
	: _staticHandler(root), _responseBuilder("Webserv/1.0", false)
{
	_handlers["GET:/"] = &_staticHandler;
	_handlers["HEAD:/"] = &_staticHandler;
	_handlers["POST:/"] = &_staticHandler;
	_handlers["DELETE:/"] = &_staticHandler;
}

Router::Router(const Router &copy)
	: _staticHandler(copy._staticHandler), _handlers(copy._handlers), _responseBuilder(copy._responseBuilder)
{
	_handlers["GET:/"] = &_staticHandler;
	_handlers["HEAD:/"] = &_staticHandler;
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
		_handlers["HEAD:/"] = &_staticHandler;
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
 * The location whose path is the longest matching prefix of the URI wins, and
 * a prefix only matches on a path boundary: "/cgi" serves "/cgi/app.py" and
 * "/cgi" itself, never "/cgi-bin/hello.php" or "/cgifoo". Matching those would
 * hand a request to a block written for a different tree, and with cgi_pass in
 * it that means executing a script the author never pointed at that path.
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
		size_t				size = locPath.size();

		if (size == 0 || uri.compare(0, size, locPath) != 0)
			continue;
		if (uri.size() != size && uri[size] != '/'
			&& locPath[size - 1] != '/')
			continue;
		if (best == NULL || size > best->path.size())
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
 * server root, and a server declaring none falls back to the default root, so
 * a URI always resolves to a root of its own instead of keeping the one the
 * previous request left on the handler.
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
	if (!config.root.empty())
		return (config.root);
	return (DEFAULT_ROOT);
}

/**
 * @brief Returns the prefix a location removes from a URI before resolving it.
 * Only a location declaring a root of its own relocates what it serves, which
 * is the mapping the subject describes: "/kapouet" rooted in "/tmp/www" serves
 * "/kapouet/pouic/toto/pouet" from "/tmp/www/pouic/toto/pouet". A location
 * inheriting the server root keeps the URI whole, so a tree laid out under a
 * single server root still resolves the way it sits on disk.
 * @param uri The request target.
 * @param config The server block serving the request.
 * @return The prefix to strip, or an empty string when nothing is stripped.
 */
std::string	Router::resolveLocationPrefix(const std::string &uri,
			const ServerConfig &config) const
{
	const LocationConfig	*best = matchLocation(uri, config);

	if (best != NULL && !best->root.empty())
		return (best->path);
	return ("");
}

/**
 * @brief Picks the index file that applies to a URI in a server block.
 * ConfigLoader already copies the server index into every location that
 * declares none, so an empty location index only happens when the server
 * declares none either, and the default index is used as the fallback rather
 * than an empty name, so a URI matching no location never keeps the index a
 * previous request resolved through one.
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
	if (!config.index.empty())
		return (config.index);
	return (DEFAULT_INDEX);
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
 * @brief Picks the upload directory that applies to a URI in a server block.
 * upload_store is declared per location, so a URI matching none accepts no
 * upload at all rather than inheriting a directory the server never named.
 * An empty result is what tells POST apart from a location that stores it.
 * @param uri The request target.
 * @param config The server block serving the request.
 * @return The directory an upload is written to, or an empty string when the
 * location matching the URI declares none.
 */
std::string	Router::resolveUploadStore(const std::string &uri,
			const ServerConfig &config) const
{
	const LocationConfig	*best = matchLocation(uri, config);

	if (best != NULL)
		return (best->uploadStore);
	return ("");
}

/**
 * @brief Reports whether the body of a request is over the size its URI allows.
 * The parser only knows the widest client_max_body_size configured for the
 * port, since the server block and location serving a request are picked from
 * headers it has not read yet. This answers the same question once both are
 * known, so a request the parser let through is still refused when the block
 * or location that ended up serving it declares a narrower limit.
 * @param request The request being answered.
 * @param config The server block serving the request.
 * @return true when the body is larger than the limit resolved for the URI.
 */
bool	Router::bodyExceedsLimit(const HttpRequest &request,
			const ServerConfig &config) const
{
	long	limit = resolveMaxBodySize(request.getUri(), config);

	return (limit >= 0
		&& request.getBody().size() > static_cast<size_t>(limit));
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
 * never turns into an unreadable body. The file is read through a C++ stream
 * rather than read() on a descriptor, because the subject forbids reading any
 * descriptor that did not go through poll() and a regular file never enters
 * the poll set.
 * @param path The file to read.
 * @param body The destination holding the file contents.
 * @return true when the whole file could be read.
 */
static bool	readErrorPageFile(const std::string &path, std::string &body)
{
	struct stat	info;
	char		buffer[4096];

	if (stat(path.c_str(), &info) == -1 || !S_ISREG(info.st_mode))
		return (false);

	std::ifstream	file(path.c_str(), std::ios::in | std::ios::binary);

	if (!file.is_open())
		return (false);
	while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0)
	{
		body.append(buffer, static_cast<size_t>(file.gcount()));
		if (file.eof())
			break ;
	}
	if (file.bad())
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
 * @brief The method a limit_except list is tested against.
 * HEAD is answered by the GET path and differs only by dropping the body, so
 * limit_except is checked as if it were GET; a location allowing GET therefore
 * allows HEAD, the way nginx treats it. Every other method is tested as
 * written.
 * @param method The request method.
 * @return The method to look up in an allowed-methods list.
 */
static std::string	limitMethod(const std::string &method)
{
	if (method == "HEAD")
		return ("GET");
	return (method);
}

/**
 * @brief Tells whether the location matching a request refuses its method.
 * Same reason as redirects(): CGI is dispatched before the router runs, so the
 * caller driving it has to know the method is refused to keep a script from
 * executing on a route that does not accept it. The 405 itself is written by
 * route(), through applyMethodLimit.
 * @param request The request being answered.
 * @param config The server block serving the request.
 * @return true when limit_except leaves the method of the request out.
 */
bool	Router::refusesMethod(const HttpRequest &request,
			const ServerConfig &config) const
{
	const LocationConfig	*best = matchLocation(request.getUri(), config);
	std::string				method = limitMethod(request.getMethod());

	if (best == NULL || best->allowedMethods.empty())
		return (false);
	for (size_t i = 0; i < best->allowedMethods.size(); ++i)
	{
		if (best->allowedMethods[i] == method)
			return (false);
	}
	return (true);
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

/**
 * @brief Joins method names into the comma-separated form of an Allow header.
 * @param methods The methods to name.
 * @return The header value listing them in order.
 */
static std::string	joinMethods(const std::vector<std::string> &methods)
{
	std::string	allow;

	for (size_t i = 0; i < methods.size(); ++i)
	{
		if (!allow.empty())
			allow.append(", ");
		allow.append(methods[i]);
	}
	return (allow);
}

/**
 * @brief Drops one method from the value of an Allow header.
 * A POST refused for want of an upload_store still has to name the methods
 * the resource does answer, which is the rest of the list it belongs to.
 * @param allow The header value to filter.
 * @param excluded The method to leave out.
 * @return The header value without the excluded method.
 */
static std::string	methodsExcept(const std::string &allow,
			const std::string &excluded)
{
	std::vector<std::string>	kept;
	size_t						start = 0;

	while (start <= allow.size())
	{
		size_t	end = allow.find(", ", start);

		if (end == std::string::npos)
			end = allow.size();

		std::string	method = allow.substr(start, end - start);

		if (!method.empty() && method != excluded)
			kept.push_back(method);
		start = end + 2;
	}
	return (joinMethods(kept));
}

/**
 * @brief Refuses a method the location matching the request does not allow.
 * A location without limit_except restricts nothing, so its empty list lets
 * every implemented method through. When the method is refused the response is
 * answered 405 carrying the Allow header the standard requires, naming the
 * methods the location does accept.
 * @param request The request being answered.
 * @param response The response to fill when the method is refused.
 * @param config The server block serving the request.
 * @return true when the response was answered 405 and no handler may run,
 * false when the method is allowed to proceed.
 */
bool	Router::applyMethodLimit(const HttpRequest &request,
			HttpResponse &response, const ServerConfig &config) const
{
	const LocationConfig	*best = matchLocation(request.getUri(), config);
	std::string				method = limitMethod(request.getMethod());

	if (best == NULL || best->allowedMethods.empty())
		return (false);
	for (size_t i = 0; i < best->allowedMethods.size(); ++i)
	{
		if (best->allowedMethods[i] == method)
			return (false);
	}
	response.setStatusCode(405);
	response.setHeaders("Allow", joinMethods(best->allowedMethods));
	response.setBody("");
	Logger::warning("limit_except refused: " + request.getMethod() + " "
			+ request.getUri());
	return (true);
}

/**
 * @brief Refuses a POST to a location that declares no upload directory.
 * An upload is only ever written where upload_store names, so a location
 * declaring none accepts none, and answering 405 keeps a POST from creating
 * or truncating a file the config never offered for writing. The refusal
 * belongs here rather than in the handler because the Allow header the status
 * requires is built from the methods the location and the router do answer,
 * which is the same list minus POST.
 * A body over the limit its URI allows is refused whatever the location does
 * with uploads, so it is left to the handler answering 413 rather than being
 * reported as a method the resource does not accept.
 * @param request The request being answered.
 * @param response The response to fill when the upload is refused.
 * @param config The server block serving the request.
 * @param implemented The methods the router answers for the URI.
 * @return true when the response was answered 405 and no handler may run,
 * false when the POST may proceed.
 */
bool	Router::applyUploadLimit(const HttpRequest &request,
			HttpResponse &response, const ServerConfig &config,
			const std::string &implemented) const
{
	if (request.getMethod() != "POST")
		return (false);
	if (bodyExceedsLimit(request, config))
		return (false);
	if (!resolveUploadStore(request.getUri(), config).empty())
		return (false);

	const LocationConfig	*best = matchLocation(request.getUri(), config);
	std::string				accepted = implemented;

	if (best != NULL && !best->allowedMethods.empty())
		accepted = joinMethods(best->allowedMethods);
	response.setStatusCode(405);
	response.setHeaders("Allow", methodsExcept(accepted, "POST"));
	response.setBody("");
	Logger::warning("upload refused, no upload_store: " + request.getUri());
	return (true);
}

bool	Router::route(const HttpRequest &request,
				HttpResponse &response, const ServerConfig &config)
{
	bool pathFound = false;
	std::string allow;

	if (applyRedirect(request, response, config))
		return (true);

	if (applyMethodLimit(request, response, config))
	{
		applyErrorPage(request, response, config);
		return (true);
	}
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
	else if (!applyUploadLimit(request, response, config, allow))
	{
		setRoot(resolveRoot(request.getUri(), config));
		setIndex(resolveIndex(request.getUri(), config));
		_staticHandler.setLocationPrefix(
				resolveLocationPrefix(request.getUri(), config));
		_staticHandler.setAutoindex(
				resolveAutoindex(request.getUri(), config));
		_staticHandler.setMaxBodySize(
				resolveMaxBodySize(request.getUri(), config));
		_staticHandler.setUploadStore(
				resolveUploadStore(request.getUri(), config));
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


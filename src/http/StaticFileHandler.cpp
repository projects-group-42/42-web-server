/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   StaticFileHandler.cpp                              :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dajesus- <dajesus-@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/22 17:24:45 by dajesus-          #+#    #+#             */
/*   Updated: 2026/06/29 21:31:35 by dajesus-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "http/StaticFileHandler.hpp"
#include "http/MimeType.hpp"
#include <limits.h>
#include <stdlib.h>
#include <vector>
#include <cerrno>

StaticFileHandler::StaticFileHandler(void)
	: _root("www"), _index("index.html")
{
}

StaticFileHandler::StaticFileHandler(const std::string &root)
	: _root(root), _index("index.html")
{
}

StaticFileHandler::StaticFileHandler(const StaticFileHandler &copy)
{
	*this = copy;
}

StaticFileHandler &StaticFileHandler::operator=(const StaticFileHandler &other)
{
	if (this != &other)
	{
		_root = other._root;
		_index = other._index;
	}
	return (*this);
}

StaticFileHandler::~StaticFileHandler(void)
{
}

void	StaticFileHandler::setRoot(const std::string &root)
{
	_root = root;
}

void	StaticFileHandler::setIndex(const std::string &index)
{
	_index = index;
}

const std::string &StaticFileHandler::getRoot(void) const
{
	return (_root);
}

/*
 * Serve a regular file, open it, read all bytes, set MIME type.
 * Returns HTTP status code.
 */
int StaticFileHandler::serveRegularFile(const std::string &resolvedPath,
		std::string &body, std::string &contentType)
{
	int	fd = open(resolvedPath.c_str(), O_RDONLY);
	if (fd == -1)
		return (403);

	char		buf[4096];
	ssize_t		bytes;
	while ((bytes = read(fd, buf, sizeof(buf))) > 0)
		body.append(buf, bytes);

	close(fd);

	contentType = mimeType_resolve(resolvedPath);
	return (200);
}

/*
 * Serve a directory, try index files.
 * Returns HTTP status code and fills body/contentType.
 */
int StaticFileHandler::serveDirectory(const std::string &resolvedPath,
		std::string &body, std::string &contentType)
{
	std::string indexPath = resolvedPath;
	if (indexPath[indexPath.size() - 1] != '/')
		indexPath += '/';
	indexPath += _index;

	struct stat	st;
	if (stat(indexPath.c_str(), &st) == 0 && S_ISREG(st.st_mode))
		return (serveRegularFile(indexPath, body, contentType));
	return (404);
}

/*
 * Return the canonical absolute path of `path`, or an empty string when it
 * cannot be resolved (for instance because it does not exist).
 */
static std::string canonicalPath(const std::string &path)
{
	char	buffer[PATH_MAX];

	if (realpath(path.c_str(), buffer) == NULL)
		return ("");
	return (std::string(buffer));
}

/*
 * Resolve `uri` into a filesystem path inside the document root.
 * The URI is split into segments, collapsing "." and ".." lexically; any ".."
 * that would climb above the root returns an empty string so the caller can
 * answer 403. When the resolved target exists, its canonical path is checked
 * against the canonical root so symlinks cannot escape the document root.
 */
std::string StaticFileHandler::rslv_req_realpath(const std::string &uri)
{
	std::vector<std::string>	segments;
	std::string					path = _root;
	size_t						i = 0;

	while (i < uri.size())
	{
		while (i < uri.size() && uri[i] == '/')
			++i;
		size_t	start = i;
		while (i < uri.size() && uri[i] != '/')
			++i;
		if (i == start)
			continue;
		std::string	segment = uri.substr(start, i - start);
		if (segment == ".")
			continue;
		if (segment == "..")
		{
			if (segments.empty())
				return ("");
			segments.pop_back();
		}
		else
			segments.push_back(segment);
	}
	for (size_t j = 0; j < segments.size(); ++j)
		path += "/" + segments[j];

	std::string	root = canonicalPath(_root);
	std::string	resolved = canonicalPath(path);
	if (!root.empty() && !resolved.empty() && resolved != root
		&& resolved.compare(0, root.size() + 1, root + "/") != 0)
		return ("");
	return (path);
}

/*
 * Serves the resource resolved from the URI as the response body.
 * Resolves the path, serves the file or directory index, and fills
 * the HttpResponse with the result.
 */
bool StaticFileHandler::handleGet(const HttpRequest &request,
		HttpResponse &response)
{
	std::string		resolvedPath = rslv_req_realpath(request.getUri());

	if (resolvedPath.empty())
	{
		response.setStatusCode(403);
		return (true);
	}

	struct stat	pathStat;
	if (stat(resolvedPath.c_str(), &pathStat) != 0)
	{
		response.setStatusCode(404);
		return (true);
	}

	std::string	body;
	std::string	contentType;
	int			status;

	if (S_ISREG(pathStat.st_mode))
		status = serveRegularFile(resolvedPath, body, contentType);
	else if (S_ISDIR(pathStat.st_mode))
		status = serveDirectory(resolvedPath, body, contentType);
	else
		status = 403;

	response.setStatusCode(status);
	response.setBody(body);
	response.setHeaders("content-type", contentType);
	return (true);
}

/*
 * Writes the request body to the file resolved from the URI.
 * Returns 201 if the file was created, 200 if it was overwritten.
 */
bool StaticFileHandler::handlePost(const HttpRequest &request,
		HttpResponse &response)
{
	std::string	resolvedPath = rslv_req_realpath(request.getUri());

	if (resolvedPath.empty())
	{
		response.setStatusCode(403);
		return (true);
	}

	struct stat	pathStat;
	bool		exists = (stat(resolvedPath.c_str(), &pathStat) == 0);

	if (exists && S_ISDIR(pathStat.st_mode))
	{
		response.setStatusCode(400);
		return (true);
	}

	int	fd = open(resolvedPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd == -1)
	{
		if (errno == EACCES)
			response.setStatusCode(403);
		else if (errno == ENOENT)
			response.setStatusCode(404);
		else
			response.setStatusCode(500);
		return (true);
	}

	const std::string	&body = request.getBody();
	ssize_t				written = write(fd, body.c_str(), body.size());
	close(fd);

	if (written == -1 || static_cast<size_t>(written) != body.size())
	{
		response.setStatusCode(500);
		return (true);
	}

	response.setStatusCode(exists ? 200 : 201);
	return (true);
}

/*
 * Removes the file resolved from the URI. Returns 204 on success,
 * 404 when the target is missing, 403 when it is a directory or the
 * removal is denied, and 500 on other failures.
 */
bool StaticFileHandler::handleDelete(const HttpRequest &request,
		HttpResponse &response)
{
	std::string	resolvedPath = rslv_req_realpath(request.getUri());

	if (resolvedPath.empty())
	{
		response.setStatusCode(403);
		return (true);
	}

	struct stat	pathStat;
	if (stat(resolvedPath.c_str(), &pathStat) != 0)
	{
		response.setStatusCode(404);
		return (true);
	}

	if (S_ISDIR(pathStat.st_mode))
	{
		response.setStatusCode(403);
		return (true);
	}

	if (unlink(resolvedPath.c_str()) == -1)
	{
		if (errno == EACCES || errno == EPERM)
			response.setStatusCode(403);
		else if (errno == ENOENT)
			response.setStatusCode(404);
		else
			response.setStatusCode(500);
		return (true);
	}

	response.setStatusCode(204);
	return (true);
}

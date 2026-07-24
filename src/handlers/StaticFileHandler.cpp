/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   StaticFileHandler.cpp                              :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dajesus- <dajesus-@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/22 17:24:45 by dajesus-          #+#    #+#             */
/*   Updated: 2026/07/24 18:23:26 by dajesus-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "handlers/StaticFileHandler.hpp"
#include "http/MimeType.hpp"
#include "http/MultipartParser.hpp"
#include "utils/Utils.hpp"
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
 * Creates or overwrites the file at `resolvedPath` with `content`.
 * Returns the HTTP status code describing the outcome: 201 when the file
 * did not exist yet, 200 when an existing file was overwritten, 400 when
 * the target is a directory, 403/404/500 on the matching write failures.
 */
int StaticFileHandler::saveFile(const std::string &resolvedPath,
		const std::string &content)
{
	struct stat	pathStat;
	bool		exists = (stat(resolvedPath.c_str(), &pathStat) == 0);

	if (exists && S_ISDIR(pathStat.st_mode))
		return (400);

	int	fd = open(resolvedPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd == -1)
	{
		if (errno == EACCES)
			return (403);
		if (errno == ENOENT)
			return (404);
		return (500);
	}

	ssize_t	written = write(fd, content.c_str(), content.size());
	close(fd);

	if (written == -1 || static_cast<size_t>(written) != content.size())
		return (500);
	return (exists ? 200 : 201);
}

/*
 * Detects a multipart/form-data POST and extracts its boundary token.
 * Returns true whenever Content-Type declares multipart/form-data, even if
 * the boundary parameter turns out to be missing or malformed, so the
 * caller can answer with the appropriate error.
 */
bool StaticFileHandler::isMultipartFormData(const HttpRequest &request,
		std::string &boundary) const
{
	std::string	contentType = request.getHeaderValue("Content-Type");
	std::string	lowerType = toLower(contentType);

	if (lowerType.compare(0, 19, "multipart/form-data") != 0)
		return (false);
	MultipartParser::extractBoundary(contentType, boundary);
	return (true);
}

/*
 * Parses a multipart/form-data body and saves every file part under the
 * directory addressed by the request URI, naming each file after its
 * Content-Disposition "filename" attribute. Form fields without a filename
 * are ignored. Returns 400 on a malformed body or when no file part is
 * present, 201/200 mirroring handlePost when at least one file is saved,
 * and 403/404/500 on the matching save failures.
 */
bool StaticFileHandler::handleMultipartUpload(const HttpRequest &request,
		const std::string &boundary, HttpResponse &response)
{
	if (boundary.empty())
	{
		response.setStatusCode(400);
		return (true);
	}

	MultipartParser	parser;
	if (!parser.parse(request.getBody(), boundary))
	{
		response.setStatusCode(parser.getErrorCode());
		return (true);
	}

	std::string	baseUri = request.getUri();
	if (baseUri.empty() || baseUri[baseUri.size() - 1] != '/')
		baseUri += "/";

	const std::vector<MultipartPart>	&parts = parser.getParts();
	size_t								savedFiles = 0;
	bool								anyCreated = false;

	for (size_t i = 0; i < parts.size(); ++i)
	{
		if (!parts[i].isFile())
			continue;

		std::string	resolvedPath = rslv_req_realpath(baseUri + parts[i].filename);
		if (resolvedPath.empty())
		{
			response.setStatusCode(403);
			return (true);
		}

		int	status = saveFile(resolvedPath, parts[i].content);
		if (status != 200 && status != 201)
		{
			response.setStatusCode(status);
			return (true);
		}
		++savedFiles;
		anyCreated = anyCreated || (status == 201);
	}

	if (savedFiles == 0)
	{
		response.setStatusCode(400);
		return (true);
	}

	response.setStatusCode(anyCreated ? 201 : 200);
	return (true);
}

/*
 * Writes the request body to the file resolved from the URI. When the
 * request carries multipart/form-data, delegates to handleMultipartUpload
 * to extract and save the file part(s) instead. Returns 201 if a file was
 * created, 200 if it was overwritten.
 */
bool StaticFileHandler::handlePost(const HttpRequest &request,
		HttpResponse &response)
{
	std::string	boundary;
	if (isMultipartFormData(request, boundary))
		return (handleMultipartUpload(request, boundary, response));

	std::string	resolvedPath = rslv_req_realpath(request.getUri());
	if (resolvedPath.empty())
	{
		response.setStatusCode(403);
		return (true);
	}

	response.setStatusCode(saveFile(resolvedPath, request.getBody()));
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
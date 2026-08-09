/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   StaticFileHandler.cpp                              :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dajesus- <dajesus-@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/22 17:24:45 by dajesus-          #+#    #+#             */
/*   Updated: 2026/08/07 19:36:02 by dajesus-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "handlers/StaticFileHandler.hpp"
#include "http/MimeType.hpp"
#include "http/MultipartParser.hpp"
#include "utils/Utils.hpp"
#include <limits.h>
#include <stdlib.h>
#include <dirent.h>
#include <algorithm>
#include <vector>
#include <cerrno>

StaticFileHandler::StaticFileHandler(void)
	: _root("www"), _index("index.html"), _autoindex(false),
	  _maxBodySize(1 * 1024 * 1024)
{
}

StaticFileHandler::StaticFileHandler(const std::string &root)
	: _root(root), _index("index.html"), _autoindex(false),
	  _maxBodySize(1 * 1024 * 1024)
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
		_autoindex = other._autoindex;
		_maxBodySize = other._maxBodySize;
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

/*
 * Enables or disables the generated directory listing served when a directory
 * holds no index file.
 */
void	StaticFileHandler::setAutoindex(bool autoindex)
{
	_autoindex = autoindex;
}

/*
 * Sets the maximum accepted request body size in bytes. A negative value
 * disables the limit; the default mirrors the server's client_max_body_size.
 */
void	StaticFileHandler::setMaxBodySize(long maxBodySize)
{
	_maxBodySize = maxBodySize;
}

const std::string &StaticFileHandler::getRoot(void) const
{
	return (_root);
}

/*
 * Serve a regular file, open it, read all bytes, set MIME type.
 * A read() failing part way through leaves the body holding only the bytes
 * read so far, so the partial content is dropped instead of being answered as
 * if it were the whole file. Returns the HTTP status code: 200 when the file
 * was read, 403 when it could not be opened, 500 when a read failed.
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
	if (bytes == -1)
	{
		body.clear();
		return (500);
	}

	contentType = mimeType_resolve(resolvedPath);
	return (200);
}

/*
 * Serve a directory, try index files. When the directory holds no index and
 * autoindex is enabled, a generated listing is served instead of 404.
 * Returns HTTP status code and fills body/contentType.
 */
int StaticFileHandler::serveDirectory(const std::string &resolvedPath,
		const std::string &requestUri,
		std::string &body, std::string &contentType)
{
	std::string indexPath = resolvedPath;
	if (indexPath[indexPath.size() - 1] != '/')
		indexPath += '/';
	indexPath += _index;

	struct stat	st;
	if (stat(indexPath.c_str(), &st) == 0 && S_ISREG(st.st_mode))
		return (serveRegularFile(indexPath, body, contentType));
	if (!_autoindex)
		return (404);

	int	status = serveDirectoryListing(resolvedPath, requestUri, body);
	if (status == 200)
		contentType = "text/html";
	return (status);
}

/*
 * Reads the names held by `resolvedPath`, dropping "." and "..", and appends a
 * '/' to the names that denote a directory. stat() is used rather than
 * dirent::d_type because the latter is not populated on every filesystem.
 * Returns false when the directory cannot be opened.
 */
static bool readDirectoryEntries(const std::string &resolvedPath,
		std::vector<std::string> &entries)
{
	DIR	*dir = opendir(resolvedPath.c_str());

	if (dir == NULL)
		return (false);

	std::string	base = resolvedPath;
	if (base.empty() || base[base.size() - 1] != '/')
		base += '/';

	struct dirent	*entry;
	while ((entry = readdir(dir)) != NULL)
	{
		std::string	name = entry->d_name;

		if (name == "." || name == "..")
			continue;

		struct stat	st;
		if (stat((base + name).c_str(), &st) == 0 && S_ISDIR(st.st_mode))
			name += '/';
		entries.push_back(name);
	}
	closedir(dir);
	std::sort(entries.begin(), entries.end());
	return (true);
}

/*
 * Generate an HTML page listing the entries of a directory.
 * Every entry becomes a link relative to the requested URI, and directories
 * keep their trailing '/' so relative URLs inside them resolve correctly.
 * The URI and the entry names are attacker-controlled, so they are
 * percent-encoded before entering an href and HTML-escaped before entering
 * the page. Returns 200, or 403 when the directory cannot be read.
 */
int StaticFileHandler::serveDirectoryListing(const std::string &resolvedPath,
		const std::string &requestUri, std::string &body)
{
	std::vector<std::string>	entries;

	if (!readDirectoryEntries(resolvedPath, entries))
		return (403);

	std::string	linkPrefix = urlEncodePath(requestUri);
	if (linkPrefix.empty() || linkPrefix[linkPrefix.size() - 1] != '/')
		linkPrefix += '/';

	std::string	title = htmlEscape(requestUri);
	std::string	listing;

	listing += "<html>\r\n<head><title>Index of ";
	listing += title;
	listing += "</title></head>\r\n<body>\r\n<h1>Index of ";
	listing += title;
	listing += "</h1>\r\n<hr>\r\n<ul>\r\n";

	for (size_t i = 0; i < entries.size(); ++i)
	{
		const std::string	&name = entries[i];
		bool				isDir = (name[name.size() - 1] == '/');
		std::string			target = linkPrefix
								+ urlEncodeSegment(isDir
									? name.substr(0, name.size() - 1) : name);

		if (isDir)
			target += '/';

		listing += "<li><a href=\"";
		listing += htmlEscape(target);
		listing += "\">";
		listing += htmlEscape(name);
		listing += "</a></li>\r\n";
	}

	listing += "</ul>\r\n<hr>\r\n</body>\r\n</html>";
	body = listing;
	return (200);
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
 * answer 403. The canonical path of the target (when it exists) or of its
 * parent directory (when it does not, as happens for every upload) is then
 * checked against the canonical root, so a symlink cannot be used to escape
 * the document root either directly or by creating a new file through it.
 * A root that cannot be canonicalised (empty or missing) is refused outright:
 * without it there is nothing to confine the request to.
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
	if (root.empty())
		return ("");

	std::string	resolved = canonicalPath(path);
	if (resolved.empty())
	{
		size_t	slash = path.find_last_of('/');
		if (slash != std::string::npos)
			resolved = canonicalPath(path.substr(0, slash));
	}
	if (!resolved.empty() && resolved != root
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
		status = serveDirectory(resolvedPath, request.getUri(), body,
				contentType);
	else
		status = 403;

	response.setStatusCode(status);
	response.setBody(body);
	response.setHeaders("content-type", contentType);
	return (true);
}

/*
 * Creates or overwrites the file at `resolvedPath` with `content`.
 * Writes in a loop so a short write() does not truncate the content. A
 * write() returning -1 or 0 is a failure: the partial file is removed
 * before reporting 500, so no truncated file is left on disk. A target
 * that already exists without being a regular file is refused before
 * open(), since opening a FIFO or a device node for writing blocks until
 * the other end is ready and would stall the event loop. Returns
 * the HTTP status code describing the outcome: 201 when the file did
 * not exist yet, 200 when an existing file was overwritten, 400 when
 * the target is a directory, 403/404/500 on the matching write
 * failures.
 */
int StaticFileHandler::saveFile(const std::string &resolvedPath,
		const std::string &content)
{
	struct stat	pathStat;
	bool		exists = (stat(resolvedPath.c_str(), &pathStat) == 0);

	if (exists && S_ISDIR(pathStat.st_mode))
		return (400);
	if (exists && !S_ISREG(pathStat.st_mode))
		return (403);

	int	fd = open(resolvedPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd == -1)
	{
		if (errno == EACCES)
			return (403);
		if (errno == ENOENT)
			return (404);
		return (500);
	}

	const char	*data = content.c_str();
	size_t		total = content.size();
	size_t		offset = 0;

	while (offset < total)
	{
		ssize_t	written = write(fd, data + offset, total - offset);
		if (written <= 0)
		{
			close(fd);
			unlink(resolvedPath.c_str());
			return (500);
		}
		offset += static_cast<size_t>(written);
	}
	close(fd);
	return (exists ? 200 : 201);
}

/*
 * Detects a multipart/form-data request by inspecting the Content-Type
 * header and extracts its boundary token. Does not check the request
 * method. Returns true whenever Content-Type declares multipart/form-data,
 * even if the boundary parameter turns out to be missing or malformed, so
 * the caller can answer with the appropriate error.
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
 * Returns the final path component of `name`, discarding everything up to
 * and including the last '/' or '\'. Strips directory components from an
 * attacker-controlled upload filename so a part can only be written inside
 * the target directory.
 */
static std::string	fileBaseName(const std::string &name)
{
	size_t	slash = name.find_last_of("/\\");

	if (slash == std::string::npos)
		return (name);
	return (name.substr(slash + 1));
}

/*
 * Parses a multipart/form-data body and saves every file part under the
 * directory addressed by the request URI, naming each file after the base
 * name of its Content-Disposition "filename" attribute. Form fields without
 * a filename are ignored. Returns 400 on a malformed body, an unsafe
 * filename, or when no file part is present, 201/200 mirroring handlePost
 * when at least one file is saved, and 403/404/500 on the matching save
 * failures.
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

		std::string	filename = fileBaseName(parts[i].filename);
		if (filename.empty() || filename == "." || filename == "..")
		{
			response.setStatusCode(400);
			return (true);
		}

		std::string	resolvedPath = rslv_req_realpath(baseUri + filename);
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
 * to extract and save the file part(s) instead. Rejects with 413 when the
 * body exceeds the configured maximum size. Returns 201 if a file was
 * created, 200 if it was overwritten.
 */
bool StaticFileHandler::handlePost(const HttpRequest &request,
		HttpResponse &response)
{
	if (_maxBodySize >= 0
		&& request.getBody().size() > static_cast<size_t>(_maxBodySize))
	{
		response.setStatusCode(413);
		return (true);
	}

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

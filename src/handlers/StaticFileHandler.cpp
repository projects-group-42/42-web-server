/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   StaticFileHandler.cpp                              :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/22 17:24:45 by dajesus-          #+#    #+#             */
/*   Updated: 2026/08/08 18:25:04 by jucoelho         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "handlers/StaticFileHandler.hpp"
#include "http/MimeType.hpp"
#include "http/MultipartParser.hpp"
#include "utils/Utils.hpp"
#include <cstdio>
#include <dirent.h>
#include <algorithm>
#include <vector>
#include <cerrno>
#include <fstream>

StaticFileHandler::StaticFileHandler(void)
	: _root("www"), _index("index.html"), _uploadStore(""),
	_locationPrefix(""), _autoindex(false), _maxBodySize(1 * 1024 * 1024)
{
}

StaticFileHandler::StaticFileHandler(const std::string &root)
	: _root(root), _index("index.html"), _uploadStore(""),
	  _locationPrefix(""), _autoindex(false),
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
		_uploadStore = other._uploadStore;
		_locationPrefix = other._locationPrefix;
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

void StaticFileHandler::setUploadStore(const std::string &uploadStore)
{
	_uploadStore = uploadStore;
}

/*
 * Sets the location prefix removed from a URI before it is resolved against
 * the root. The Router fills it per request; an empty value keeps the URI
 * whole.
 */
void StaticFileHandler::setLocationPrefix(const std::string &prefix)
{
	_locationPrefix = prefix;
}

const std::string &StaticFileHandler::getRoot(void) const
{
	return (_root);
}

/*
 * Serve a regular file, open it, read all bytes, set MIME type.
 * The file is read through a C++ stream rather than read() on a descriptor:
 * the subject forbids reading any file descriptor that did not go through
 * poll(), and a regular file never enters the poll set. Returns HTTP status
 * code.
 */
int StaticFileHandler::serveRegularFile(const std::string &resolvedPath,
		std::string &body, std::string &contentType)
{
	std::ifstream	file(resolvedPath.c_str(),
					std::ios::in | std::ios::binary);

	if (!file.is_open())
		return (403);

	char	buf[4096];

	while (file.read(buf, sizeof(buf)) || file.gcount() > 0)
	{
		body.append(buf, static_cast<size_t>(file.gcount()));
		if (file.eof())
			break ;
	}
	if (file.bad())
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
 * Removes the prefix of the location serving the request from `uri`, so the
 * rest of it is what gets resolved against the root that location declares.
 * This is the mapping the subject asks for: "/kapouet" rooted in "/tmp/www"
 * serves "/kapouet/pouic/toto/pouet" from "/tmp/www/pouic/toto/pouet". The
 * prefix is only dropped on a path boundary, so "/cgi" never eats into
 * "/cgifoo", and an empty prefix (a location inheriting the server root, or a
 * handler used outside the router) leaves the URI whole.
 */
std::string StaticFileHandler::stripLocationPrefix(const std::string &uri) const
{
	size_t	size = _locationPrefix.size();

	if (size == 0 || uri.compare(0, size, _locationPrefix) != 0)
		return (uri);
	if (uri.size() == size || uri[size] == '/'
		|| _locationPrefix[size - 1] == '/')
		return (uri.substr(size));
	return (uri);
}

/*
 * Resolve `uri` into a filesystem path inside the document root.
 * The URI is split into segments, collapsing "." and ".." lexically; any ".."
 * that would climb above the root returns an empty string so the caller can
 * answer 403. The target (when it exists) or its parent directory (when it
 * does not, as happens for every upload) is then checked against the root with
 * pathIsInsideRoot, so a symlink cannot be used to escape the document root
 * either directly or by creating a new file through it. A root that is missing
 * from disk is refused outright: without it there is nothing to confine the
 * request to.
 */
std::string StaticFileHandler::rslv_req_realpath(const std::string &uri)
{
	std::vector<std::string>	segments;
	std::string					target = stripLocationPrefix(uri);
	std::string					path = _root;
	size_t						i = 0;

	while (i < target.size())
	{
		while (i < target.size() && target[i] == '/')
			++i;
		size_t	start = i;
		while (i < target.size() && target[i] != '/')
			++i;
		if (i == start)
			continue;
		std::string	segment = target.substr(start, i - start);
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

	if (!pathIsInsideRoot(_root, path))
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
 * The file is written through a C++ stream rather than write() on a
 * descriptor, because the subject forbids writing any file descriptor that did
 * not go through poll() and a regular file never enters the poll set. A stream
 * that fails mid-write leaves a truncated file behind, so it is removed before
 * reporting 500. Returns the HTTP status code describing the outcome: 201 when
 * the file did not exist yet, 200 when an existing file was overwritten, 400
 * when the target is a directory, 403 when it exists but cannot be opened, 404
 * when its directory does not accept it, 500 on a failed write.
 */
int StaticFileHandler::saveFile(const std::string &resolvedPath,
		const std::string &content)
{
	struct stat	pathStat;
	bool		exists = (stat(resolvedPath.c_str(), &pathStat) == 0);

	if (exists && S_ISDIR(pathStat.st_mode))
		return (400);

	std::ofstream	file(resolvedPath.c_str(),
					std::ios::out | std::ios::binary | std::ios::trunc);

	if (!file.is_open())
		return (exists ? 403 : 404);
	file.write(content.data(), static_cast<std::streamsize>(content.size()));
	file.flush();
	if (!file.good())
	{
		file.close();
		std::remove(resolvedPath.c_str());
		return (500);
	}
	file.close();
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
 * Checks that the location declares an upload directory and that it exists on
 * disk, and hands the caller the path to write into. realpath() is not
 * authorised by the subject, so the directory is taken as the config wrote it
 * and only its existence is confirmed with stat().
 */
int StaticFileHandler::prepareUploadStore(std::string &canStore)
{
    struct stat storeStat;

    if (_uploadStore.empty())
        return 405; // no upload_store configured

    if (stat(_uploadStore.c_str(), &storeStat) != 0
        || !S_ISDIR(storeStat.st_mode))
        return 404; // upload_store missing or not a directory

    canStore = _uploadStore;
    return 200;
}

int StaticFileHandler::savePartToUploadStore(const std::string &canStore,
        const std::string &filename, const std::string &content)
{
    if (filename.empty() || filename == "." || filename == "..")
        return 400;

    std::string target = canStore;
    if (target[target.size() - 1] != '/')
        target += '/';
    target += filename;
    return saveFile(target, content);
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
	std::string canStore;
	int prep = prepareUploadStore(canStore);
	if (prep != 200)
	{
		response.setStatusCode(prep);
		return true;
	}
	const std::vector<MultipartPart>	&parts = parser.getParts();
	size_t								savedFiles = 0;
	bool								anyCreated = false;
	for (size_t i = 0; i < parts.size(); ++i)
	{
		if (!parts[i].isFile())
			continue;

		std::string	filename = fileBaseName(parts[i].filename);
		int status = savePartToUploadStore(canStore, filename, parts[i].content);
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
	if (_uploadStore.empty()) {
		response.setStatusCode(405);
		return true;
	}
	std::string	boundary;
	if (isMultipartFormData(request, boundary))
		return (handleMultipartUpload(request, boundary, response));
	 std::string filename = fileBaseName(request.getUri());
	if (filename.empty() || filename == "." || filename == "..")
	{
		response.setStatusCode(400);
		return (true);
	}
	std::string canStore;
	int			prep = prepareUploadStore(canStore);

	if (prep != 200)
	{
		response.setStatusCode(prep);
		return (true);
	}

	std::string target = canStore;
	if (target[target.size() - 1] != '/')
		target += '/';
	target += filename;
	response.setStatusCode(saveFile(target, request.getBody()));
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

	if (std::remove(resolvedPath.c_str()) == -1)
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

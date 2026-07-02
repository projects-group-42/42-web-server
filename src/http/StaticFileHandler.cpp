/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   StaticFileHandler.cpp                              :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/22 17:24:45 by dajesus-          #+#    #+#             */
/*   Updated: 2026/07/02 15:36:13 by jucoelho         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "http/StaticFileHandler.hpp"
#include "http/MimeType.hpp"
#include <filesystem>
#include <limits.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>

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

bool resolve_request_path_realpath(const std::string &root,
	const std::string &uri,std::string &out_resolved, int &out_status)
{
	// build combined path
	std::string combined = root;
	if (combined.empty() || combined[combined.size()-1] != '/')
		combined += '/';
	std::string path = uri;
	if (!path.empty() && path[0] == '/')
		path = path.substr(1);
	combined += path;

	// canonicalize root
	char root_real[PATH_MAX];
	if (realpath(root.c_str(), root_real) == NULL) {
		out_status = 500; // internal error resolving root
		return false;
	}

	// try canonicalize target
	char target_real[PATH_MAX];
	if (realpath(combined.c_str(), target_real) != NULL) {
		out_resolved = std::string(target_real);
	} else {
		if (errno != ENOENT) {
			out_status = 403; // other error -> forbid
			return false;
		}
		// target doesn't exist: canonicalize parent and append basename
		std::string parent = combined;
		std::string basename;
		size_t pos = parent.rfind('/');
		if (pos == std::string::npos) {
			parent = ".";
			basename = combined;
		} else {
			basename = parent.substr(pos + 1);
			parent = parent.substr(0, pos);
			if (parent.empty()) parent = "/";
		}

		char parent_real[PATH_MAX];
		if (realpath(parent.c_str(), parent_real) == NULL) {
			out_status = 403; // cannot resolve parent -> forbid
			return false;
		}
		out_resolved = std::string(parent_real);
		if (out_resolved[out_resolved.size() - 1] != '/')
			out_resolved += '/';
		out_resolved += basename;
	}

	// normalize root string for prefix check (no trailing slash unless root == "/")
	std::string rootStr(root_real);
	if (rootStr.size() > 1 && rootStr[rootStr.size() - 1] == '/')
		rootStr.erase(rootStr.size() - 1);

	// ensure containment: out_resolved must be inside rootStr
	if (out_resolved.compare(0, rootStr.size(), rootStr) != 0 ||
		(out_resolved.size() > rootStr.size() && out_resolved[rootStr.size()] != '/'))
	{
		out_status = 403;
		return false;
	}

	out_status = 200;
	return true;
}

/*
 * Main entry-point.
 * Parse the raw request, resolve the path, serve the file, build the HTTP response.
 */
bool StaticFileHandler::handle(const HttpRequest &request,
		HttpResponse &response)
{
	std::string	resolvedPath ;
	int			status;

	if (!resolve_request_path_realpath(_root, request.getUri(), resolvedPath, status))
	{
		response.setStatusCode(status);
		return true;
	}

	struct stat	pathStat;
	if (stat(resolvedPath.c_str(), &pathStat) != 0)
	{
		response.setStatusCode(404);
		return (true);
	}

	std::string	body;
	std::string	contentType;

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

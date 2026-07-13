/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   StaticFileHandler.cpp                              :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/22 17:24:45 by dajesus-          #+#    #+#             */
/*   Updated: 2026/07/13 17:14:54 by jucoelho         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "http/StaticFileHandler.hpp"
#include "http/MimeType.hpp"
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
std::string StaticFileHandler::rslv_req_realpath(const std::string &uri)
{
	std::string	path = _root + '/' + uri;
	size_t pos_above;
	size_t pos_actual;

	while (( pos_above = path.find("/..")) != std::string::npos)
	{
		size_t subpos = path.rfind("/", pos_above -1);
		if (subpos != std::string::npos)
		{
			path.erase(subpos, pos_above + 3 -subpos);
		}
		else
		{
			path = path.substr(pos_above + 3);
		}
	}
	while ((pos_actual= path.find("/.")) != std::string::npos)
	{
		path = path.erase(pos_actual, 2);
	}
	if (path.compare(0, _root.size(), _root) != 0)
		return ("");
	return (path);
}

/*
 * Main entry-point.
 * Parse the raw request, resolve the path, serve the file, build the HTTP response.
 */
bool StaticFileHandler::handle(const HttpRequest &request,
		HttpResponse &response)
{
	std::string	resolved_path ;
	int			status;

	resolved_path = rslv_req_realpath(request.getUri());
	if (resolved_path == "")
	{
		response.setStatusCode(403);
		return true;
	}

	struct stat	pathStat;
	if (stat(resolved_path.c_str(), &pathStat) != 0)
	{
		response.setStatusCode(404);
		return (true);
	}
	
	std::string	body;
	std::string	contentType;

	if (S_ISREG(pathStat.st_mode))
		status = serveRegularFile(resolved_path, body, contentType);
	else if (S_ISDIR(pathStat.st_mode))
		status = serveDirectory(resolved_path, body, contentType);
	else
		status = 403;

	response.setStatusCode(status);
	response.setBody(body);
	response.setHeaders("content-type", contentType);
	return (true);
}

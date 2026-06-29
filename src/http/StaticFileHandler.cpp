/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   StaticFileHandler.cpp                              :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dajesus- <dajesus-@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/22 17:24:45 by dajesus-          #+#    #+#             */
<<<<<<< HEAD
/*   Updated: 2026/06/29 18:35:52 by jucoelho         ###   ########.fr       */
=======
/*   Updated: 2026/06/29 16:50:08 by dajesus-         ###   ########.fr       */
>>>>>>> origin/refactor/project-structure
/*                                                                            */
/* ************************************************************************** */

#include "http/StaticFileHandler.hpp"
#include "http/MimeType.hpp"
<<<<<<< HEAD
=======
#include "http/HttpResponse.hpp"
#include "utils/Utils.hpp"
>>>>>>> origin/refactor/project-structure

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
<<<<<<< HEAD
=======
 * Simple HTTP-date string for the Date header.
 */
std::string StaticFileHandler::buildResponse(int status, const std::string &contentType,
		const std::string &body, bool keepAlive) const
{
	HttpResponse		respModel;
	std::ostringstream	resp;
	respModel.setStatus(status);
	resp << "HTTP/1.1 " << status << " "
		 << respModel.getStatusMessage() << "\r\n";
	resp << "Content-Length: " << body.size() << "\r\n";
	resp << "Content-Type: " << contentType << "\r\n";
	resp << "Date: " << getHttpDate() << "\r\n";
	resp << "Server: webserv/1.0\r\n";
	resp << "Connection: " << (keepAlive ? "keep-alive" : "close") << "\r\n";
	resp << "\r\n";
	if (!body.empty())
		resp << body;
	return (resp.str());
}

/*
 * Minimal HTML error body for common status codes.
 */
std::string StaticFileHandler::buildErrorBody(int status, const std::string &statusMsg) const
{
	(void)statusMsg;
	HttpResponse tmp;
	tmp.setStatus(status);
	std::string msg = tmp.getStatusMessage();
	std::ostringstream body;
	body << "<!DOCTYPE html>\n"
		 << "<html>\n<head><title>" << status << " " << msg
		 << "</title></head>\n<body>\n<center><h1>"
		 << status << " " << msg
		 << "</h1></center>\n<hr><center>webserv/1.0</center>\n</body>\n</html>\n";
	return (body.str());
}

/*
>>>>>>> origin/refactor/project-structure
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
 * Main entry-point.
 * Parse the raw request, resolve the path, serve the file, build the HTTP response.
 */
<<<<<<< HEAD
HttpResponse StaticFileHandler::handleGet(const HttpRequest &request)
=======
bool StaticFileHandler::handle(const HttpRequest &request,
		std::string &response)
>>>>>>> origin/refactor/project-structure
{
	HttpResponse	response;
	std::string		resolvedPath = _root + request.getUri();

	if (resolvedPath.find("..") != std::string::npos)
	{
		response.setStatusCode(403);
		return (response);
	}

	struct stat	pathStat;
	if (stat(resolvedPath.c_str(), &pathStat) != 0)
	{
		response.setStatusCode(404);
		return (response);
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
	return (response);
}

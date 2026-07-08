/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   PostHandler.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dajesus- <dajesus-@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/08 00:00:00 by dajesus-          #+#    #+#             */
/*   Updated: 2026/07/08 00:00:00 by dajesus-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "http/PostHandler.hpp"
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>

/*
 * Default constructor. Root defaults to "www".
 */
PostHandler::PostHandler(void)
	: _root("www")
{
}

/*
 * Constructs a PostHandler rooted at the given directory.
 */
PostHandler::PostHandler(const std::string &root)
	: _root(root)
{
}

/*
 * Copy constructor.
 */
PostHandler::PostHandler(const PostHandler &copy)
{
	*this = copy;
}

/*
 * Copy assignment operator.
 */
PostHandler &PostHandler::operator=(const PostHandler &other)
{
	if (this != &other)
		_root = other._root;
	return (*this);
}

/*
 * Destructor.
 */
PostHandler::~PostHandler(void)
{
}

/*
 * Sets the root directory used to resolve request URIs.
 */
void	PostHandler::setRoot(const std::string &root)
{
	_root = root;
}

/*
 * Returns the root directory used to resolve request URIs.
 */
const std::string &PostHandler::getRoot(void) const
{
	return (_root);
}

/*
 * Writes the request body to the file resolved from the URI.
 * Returns 201 if the file was created, 200 if it was overwritten.
 */
bool PostHandler::handle(const HttpRequest &request,
		HttpResponse &response)
{
	std::string	resolvedPath = _root + request.getUri();

	if (resolvedPath.find("..") != std::string::npos)
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

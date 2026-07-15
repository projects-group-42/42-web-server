/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   CgiHandler.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dajesus- <dajesus-@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/08 00:00:00 by dajesus-          #+#    #+#             */
/*   Updated: 2026/07/08 00:00:00 by dajesus-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "cgi/CgiHandler.hpp"
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <vector>

/*
 * Default constructor. CGI root defaults to "cgi-bin".
 */
CgiHandler::CgiHandler(void)
	: _cgiRoot("cgi-bin")
{
}

/*
 * Constructs a CgiHandler rooted at the given CGI directory.
 */
CgiHandler::CgiHandler(const std::string &cgiRoot)
	: _cgiRoot(cgiRoot)
{
}

/*
 * Copy constructor.
 */
CgiHandler::CgiHandler(const CgiHandler &copy)
{
	*this = copy;
}

/*
 * Copy assignment operator.
 */
CgiHandler &CgiHandler::operator=(const CgiHandler &other)
{
	if (this != &other)
		_cgiRoot = other._cgiRoot;
	return (*this);
}

/*
 * Destructor.
 */
CgiHandler::~CgiHandler(void)
{
}

/*
 * Sets the CGI root directory used to resolve scripts.
 */
void	CgiHandler::setCgiRoot(const std::string &cgiRoot)
{
	_cgiRoot = cgiRoot;
}

/*
 * Returns the CGI root directory used to resolve scripts.
 */
const std::string &CgiHandler::getCgiRoot(void) const
{
	return (_cgiRoot);
}

/*
 * Returns true if uri ends with the given extension.
 */
bool CgiHandler::hasExtension(const std::string &uri,
		const std::string &extension) const
{
	if (uri.size() < extension.size())
		return (false);
	return (uri.compare(uri.size() - extension.size(),
			extension.size(), extension) == 0);
}

/*
 * Returns the canonical absolute path of path, or an empty string when it
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
 * Resolves the script path for a URI under the CGI root. The URI is split
 * into segments, collapsing "." and ".." lexically; any ".." that would
 * climb above the root returns an empty string. When the resolved target
 * exists, its canonical path is checked against the canonical CGI root so
 * symlinks cannot escape it. Returns an empty string on escape.
 */
std::string CgiHandler::resolvePath(const std::string &uri) const
{
	std::vector<std::string>	segments;
	std::string					path = _cgiRoot;
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

	std::string	root = canonicalPath(_cgiRoot);
	std::string	resolved = canonicalPath(path);
	if (!root.empty() && !resolved.empty() && resolved != root
		&& resolved.compare(0, root.size() + 1, root + "/") != 0)
		return ("");
	return (path);
}

/*
 * Returns true if the URI's extension identifies a CGI request.
 */
bool CgiHandler::isCgiRequest(const std::string &uri) const
{
	return (hasExtension(uri, ".php") || hasExtension(uri, ".py"));
}

/*
 * Validates the script resolved from the URI (path stays inside the CGI
 * root, exists, is a regular file, is readable). Sets the response status
 * code and returns false when validation fails.
 */
bool CgiHandler::validate(const std::string &uri,
		HttpResponse &response) const
{
	std::string	scriptPath = resolvePath(uri);

	if (scriptPath.empty())
	{
		response.setStatusCode(403);
		return (false);
	}

	struct stat	scriptStat;
	if (stat(scriptPath.c_str(), &scriptStat) != 0)
	{
		response.setStatusCode(404);
		return (false);
	}
	if (!S_ISREG(scriptStat.st_mode))
	{
		response.setStatusCode(403);
		return (false);
	}
	if (access(scriptPath.c_str(), R_OK) != 0)
	{
		response.setStatusCode(403);
		return (false);
	}
	return (true);
}

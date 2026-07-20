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

CgiHandler::CgiHandler(void)
	: _cgiRoot("cgi-bin")
{
}

CgiHandler::CgiHandler(const std::string &cgiRoot)
	: _cgiRoot(cgiRoot)
{
}

CgiHandler::CgiHandler(const CgiHandler &copy)
{
	*this = copy;
}

CgiHandler &CgiHandler::operator=(const CgiHandler &other)
{
	if (this != &other)
		_cgiRoot = other._cgiRoot;
	return (*this);
}

CgiHandler::~CgiHandler(void)
{
}

void	CgiHandler::setCgiRoot(const std::string &cgiRoot)
{
	_cgiRoot = cgiRoot;
}

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
 * Splits uri into normalized path segments. Returns false when a
 * ".." would climb above the root, true otherwise.
 */
static bool normalizeSegments(const std::string &uri,
		std::vector<std::string> &segments)
{
	size_t	counter = 0;
	size_t	start;
	std::string	segment;

	while (counter < uri.size())
	{
		while (counter < uri.size() && uri[counter] == '/')
		    ++counter;
		start = counter;

		while (counter < uri.size() && uri[counter] != '/')
		    ++counter;
		if (counter == start)
			continue;

		segment = uri.substr(start, counter - start);
		if (segment == ".")
			continue;
		if (segment == "..")
		{
			if (segments.empty())
				return (false);
			segments.pop_back();
		}
		else
			segments.push_back(segment);
	}
	return (true);
}

/*
 * Joins root and the normalized segments into a single slash-separated path.
 */
static std::string joinPath(const std::string &root,
		const std::vector<std::string> &segments)
{
	std::string	path = root;

	for (size_t counter = 0; counter < segments.size(); ++counter)
		path += "/" + segments[counter];
	return (path);
}

/*
 * Returns true when path canonically resolves inside root, so symlinks
 * cannot escape it. Paths that cannot be canonicalized are treated as inside.
 */
static bool isWithinRoot(const std::string &root, const std::string &path)
{
	std::string	canonicalRoot = canonicalPath(root);
	std::string	resolved = canonicalPath(path);

	if ((canonicalRoot.empty() || resolved.empty()) || resolved == canonicalRoot)
		return (true);
	return (resolved.compare(0, canonicalRoot.size() + 1, canonicalRoot + "/") == 0);
}

/*
 * Resolves the script path for a URI under the CGI root. The URI is
 * normalized, joined onto the root, and checked so symlinks cannot escape it.
 * Returns an empty string on escape.
 */
std::string CgiHandler::resolvePath(const std::string &uri) const
{
	std::vector<std::string>	segments;

	if (!normalizeSegments(uri, segments))
		return ("");
	std::string	path = joinPath(_cgiRoot, segments);
	if (!isWithinRoot(_cgiRoot, path))
		return ("");
	return (path);
}

bool CgiHandler::isCgiRequest(const std::string &uri) const
{
	return (hasExtension(uri, ".py"));
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

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
 * Resolves the script path for a URI under the CGI root.
 */
std::string CgiHandler::resolvePath(const std::string &uri) const
{
	return (_cgiRoot + uri);
}

/*
 * Returns true if the URI's extension identifies a CGI request.
 */
bool CgiHandler::isCgiRequest(const std::string &uri) const
{
	return (hasExtension(uri, ".php") || hasExtension(uri, ".py"));
}

/*
 * Validates the script resolved from the URI (exists, is a
 * regular file, is readable). Sets the response status code
 * and returns false when validation fails.
 */
bool CgiHandler::validate(const std::string &uri,
		HttpResponse &response) const
{
	std::string	scriptPath = resolvePath(uri);

	if (scriptPath.find("..") != std::string::npos)
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

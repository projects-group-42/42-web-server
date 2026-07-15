/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   CgiHandler.hpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dajesus- <dajesus-@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/08 00:00:00 by dajesus-          #+#    #+#             */
/*   Updated: 2026/07/08 00:00:00 by dajesus-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CGI_HANDLER_HPP
# define CGI_HANDLER_HPP

# include "http/HttpResponse.hpp"
# include <string>

/*
 * CgiHandler
 *
 * Detects whether a URI should be treated as a CGI request and runs
 * the basic validations needed before execution. Does not execute
 * the CGI script itself.
 */
class CgiHandler
{
	private:
		std::string	_cgiRoot;

		/*
		 * Returns true if uri ends with the given extension.
		 */
		bool			hasExtension(const std::string &uri,
							const std::string &extension) const;

		/*
		 * Resolves the script path for a URI under the CGI root,
		 * collapsing "." and ".." and rejecting any path that escapes
		 * the root. Returns an empty string when the path escapes.
		 */
		std::string		resolvePath(const std::string &uri) const;

	public:
		/*
		 * Default constructor. CGI root defaults to "cgi-bin".
		 */
		CgiHandler(void);

		/*
		 * Constructs a CgiHandler rooted at the given CGI directory.
		 */
		explicit CgiHandler(const std::string &cgiRoot);

		/*
		 * Copy constructor.
		 */
		CgiHandler(const CgiHandler &copy);

		/*
		 * Copy assignment operator.
		 */
		CgiHandler &operator=(const CgiHandler &other);

		/*
		 * Destructor.
		 */
		~CgiHandler(void);

		/*
		 * Sets the CGI root directory used to resolve scripts.
		 */
		void				setCgiRoot(const std::string &cgiRoot);

		/*
		 * Returns the CGI root directory used to resolve scripts.
		 */
		const std::string	&getCgiRoot(void) const;

		/*
		 * Returns true if the URI's extension identifies a CGI request.
		 */
		bool				isCgiRequest(const std::string &uri) const;

		/*
		 * Validates the script resolved from the URI (stays inside the
		 * CGI root, exists, is a regular file, is readable). Sets the
		 * response status code and returns false when validation fails.
		 */
		bool				validate(const std::string &uri,
								HttpResponse &response) const;
};

#endif

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
 * the basic validations needed before execution.
 */
class CgiHandler
{
	private:
		std::string	_cgiRoot;
		std::string		resolvePath(const std::string &uri) const;
		bool    hasExtension(const std::string &uri, const std::string &extension) const;

	public:
		CgiHandler(void);
		explicit CgiHandler(const std::string &cgiRoot);
		CgiHandler(const CgiHandler &copy);
		CgiHandler &operator=(const CgiHandler &other);
		~CgiHandler(void);

		void				setCgiRoot(const std::string &cgiRoot);
		const std::string	&getCgiRoot(void) const;
		bool				isCgiRequest(const std::string &uri) const;
		bool				validate(const std::string &uri, HttpResponse &response) const;
};

#endif

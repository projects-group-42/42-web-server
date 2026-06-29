/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   StaticFileHandler.hpp                              :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dajesus- <dajesus-@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/22 17:27:30 by dajesus-          #+#    #+#             */
/*   Updated: 2026/06/29 16:50:29 by dajesus-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef STATIC_FILE_HANDLER_HPP
# define STATIC_FILE_HANDLER_HPP

#include "http/IRequestHandler.hpp"
#include "http/HttpRequest.hpp"
#include <string>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sstream>
#include <cstdio>

class StaticFileHandler : public IRequestHandler
{
	private:
		std::string	_root;
		std::string	_index;

		int			serveRegularFile(const std::string &resolvedPath,
						std::string &body, std::string &contentType);
		int			serveDirectory(const std::string &resolvedPath,
						std::string &body, std::string &contentType);

	public:
		StaticFileHandler(void);
		explicit StaticFileHandler(const std::string &root);
		StaticFileHandler(const StaticFileHandler &copy);
		StaticFileHandler &operator=(const StaticFileHandler &other);
		~StaticFileHandler(void);

		void				setRoot(const std::string &root);
		void				setIndex(const std::string &index);
		const std::string	&getRoot(void) const;

		// Parse a raw HTTP GET request buffer and produce an HTTP response string.
		// Returns true if request was well-formed enough to produce *some* response.
		bool				handle(const HttpRequest &request,
								std::string &response);
		std::string	buildResponse(int status, const std::string &contentType,
		const std::string &body, bool keepAlive) const;
		std::string	buildErrorBody(int status, const std::string &statusMsg) const;
};

#endif

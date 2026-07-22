/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   StaticFileHandler.hpp                              :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/22 17:27:30 by dajesus-          #+#    #+#             */
/*   Updated: 2026/07/19 14:40:46 by jucoelho         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef STATIC_FILE_HANDLER_HPP
# define STATIC_FILE_HANDLER_HPP

#include "handlers/IRequestHandler.hpp"
#include "http/HttpResponse.hpp"
#include "http/HttpRequest.hpp"
#include <string>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sstream>
#include <cstdio>

/*
 * StaticFileHandler
 *
 * Serves a filesystem resource under the document root. GET reads the
 * target file or directory index, POST writes the request body to the
 * target file, and DELETE removes the target file.
 */
class StaticFileHandler : public IRequestHandler
{
	private:
		std::string	_root;
		std::string	_index;

		int			serveRegularFile(const std::string &resolvedPath,
						std::string &body, std::string &contentType);
		int			serveDirectory(const std::string &resolvedPath,
						std::string &body, std::string &contentType);
		std::string	rslv_req_realpath(const std::string &uri);

	protected:
		/*
		 * Serves the resource resolved from the URI as the response body.
		 */
		bool				handleGet(const HttpRequest &request,
								  HttpResponse &response);

		/*
		 * Writes the request body to the file resolved from the URI.
		 * Returns 201 if the file was created, 200 if it was overwritten.
		 */
		bool				handlePost(const HttpRequest &request,
								  HttpResponse &response);

		/*
		 * Removes the file resolved from the URI. Returns 200 on success.
		 */
		bool				handleDelete(const HttpRequest &request,
								  HttpResponse &response);

	public:
		StaticFileHandler(void);
		explicit StaticFileHandler(const std::string &root);
		StaticFileHandler(const StaticFileHandler &copy);
		StaticFileHandler &operator=(const StaticFileHandler &other);
		~StaticFileHandler(void);

		void				setRoot(const std::string &root);
		void				setIndex(const std::string &index);
		const std::string	&getRoot(void) const;
};

#endif

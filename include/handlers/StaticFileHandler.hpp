/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   StaticFileHandler.hpp                              :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dajesus- <dajesus-@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/22 17:27:30 by dajesus-          #+#    #+#             */
/*   Updated: 2026/07/24 18:21:08 by dajesus-         ###   ########.fr       */
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
 * target file or directory index, POST writes the request body into the
 * upload directory the location declares, and DELETE removes the target
 * file.
 */
class StaticFileHandler : public IRequestHandler
{
	private:
		std::string	_root;
		std::string	_index;
		std::string	_uploadStore;
		bool		_autoindex;
		long		_maxBodySize;

		int			serveRegularFile(const std::string &resolvedPath,
						std::string &body, std::string &contentType);
		int			serveDirectory(const std::string &resolvedPath,
						const std::string &requestUri,
						std::string &body, std::string &contentType);
		int			serveDirectoryListing(const std::string &resolvedPath,
						const std::string &requestUri, std::string &body);
		std::string	rslv_req_realpath(const std::string &uri);
		int			resolveUploadTarget(const std::string &filename,
						std::string &target) const;
		int			saveFile(const std::string &resolvedPath,
						const std::string &content);
		bool		isMultipartFormData(const HttpRequest &request,
						std::string &boundary) const;
		bool		handleMultipartUpload(const HttpRequest &request,
						const std::string &boundary, HttpResponse &response);

	protected:
		/*
		 * Serves the resource resolved from the URI as the response body.
		 */
		bool				handleGet(const HttpRequest &request,
								  HttpResponse &response);

		/*
		 * Writes the request body into the upload directory, under the base
		 * name of the URI. Returns 201 if the file was created, 200 if it
		 * was overwritten, and 405 when no upload directory is configured.
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
		void				setAutoindex(bool autoindex);
		void				setMaxBodySize(long maxBodySize);
		void				setUploadStore(const std::string &uploadStore);
		const std::string	&getRoot(void) const;
};

#endif

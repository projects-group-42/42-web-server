/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   StaticFileHandler.hpp                              :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/22 17:27:30 by dajesus-          #+#    #+#             */
/*   Updated: 2026/08/08 18:14:46 by jucoelho         ###   ########.fr       */
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
		std::string _uploadStore;
		std::string	_locationPrefix;
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
		int			saveFile(const std::string &resolvedPath,
						const std::string &content);
		bool		isMultipartFormData(const HttpRequest &request,
						std::string &boundary) const;
		bool		handleMultipartUpload(const HttpRequest &request,
						const std::string &boundary, HttpResponse &response);
		int prepareUploadStore(std::string &canStore);
		int savePartToUploadStore(const std::string &canStore,
						const std::string &filename, const std::string &content);
	protected:
		bool				handleGet(const HttpRequest &request,
								  HttpResponse &response);
		bool				handlePost(const HttpRequest &request,
								  HttpResponse &response);
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
		void				setLocationPrefix(const std::string &prefix);
		const std::string	&getRoot(void) const;
};

#endif

/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   PostHandler.hpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dajesus- <dajesus-@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/08 00:00:00 by dajesus-          #+#    #+#             */
/*   Updated: 2026/07/08 00:00:00 by dajesus-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef POST_HANDLER_HPP
# define POST_HANDLER_HPP

#include "http/IRequestHandler.hpp"
#include "http/HttpResponse.hpp"
#include "http/HttpRequest.hpp"
#include <string>

/*
 * PostHandler
 *
 * Handles POST requests by writing the request body to a file
 * resolved from the URI under the configured root.
 */
class PostHandler : public IRequestHandler
{
	private:
		std::string	_root;

	public:
		/*
		 * Default constructor. Root defaults to "www".
		 */
		PostHandler(void);

		/*
		 * Constructs a PostHandler rooted at the given directory.
		 */
		explicit PostHandler(const std::string &root);

		/*
		 * Copy constructor.
		 */
		PostHandler(const PostHandler &copy);

		/*
		 * Copy assignment operator.
		 */
		PostHandler &operator=(const PostHandler &other);

		/*
		 * Destructor.
		 */
		~PostHandler(void);

		/*
		 * Sets the root directory used to resolve request URIs.
		 */
		void				setRoot(const std::string &root);

		/*
		 * Returns the root directory used to resolve request URIs.
		 */
		const std::string	&getRoot(void) const;

		/*
		 * Writes the request body to the file resolved from the URI.
		 * Returns 201 if the file was created, 200 if it was overwritten.
		 */
		bool				handle(const HttpRequest &request,
								  HttpResponse &response);
};

#endif

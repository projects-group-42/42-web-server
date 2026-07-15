/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   IRequestHandler.hpp                                :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dajesus- <dajesus-@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/29 16:22:14 by dajesus-          #+#    #+#             */
/*   Updated: 2026/06/29 21:32:42 by dajesus-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef IREQUEST_HANDLER_HPP
# define IREQUEST_HANDLER_HPP

# include <string>

class HttpRequest;
class HttpResponse;

class IRequestHandler
{
	protected:
		/*
		 * Handles a GET request. Defaults to 405 Method Not Allowed.
		 */
		virtual bool	handleGet(const HttpRequest &request,
							HttpResponse &response);

		/*
		 * Handles a POST request. Defaults to 405 Method Not Allowed.
		 */
		virtual bool	handlePost(const HttpRequest &request,
							HttpResponse &response);

		/*
		 * Handles a DELETE request. Defaults to 405 Method Not Allowed.
		 */
		virtual bool	handleDelete(const HttpRequest &request,
							HttpResponse &response);

	public:
		/*
		 * Destructor.
		 */
		virtual ~IRequestHandler();

		/*
		 * Dispatches the request to the method matching its HTTP verb.
		 * Unsupported verbs yield 405 Method Not Allowed.
		 */
		bool			handle(const HttpRequest &request,
							HttpResponse &response);
};

#endif

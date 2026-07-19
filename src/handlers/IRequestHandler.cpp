/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   IRequestHandler.cpp                                :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/29 16:22:14 by dajesus-          #+#    #+#             */
/*   Updated: 2026/07/19 14:46:00 by jucoelho         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "handlers/IRequestHandler.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"

/*
 * Destructor.
 */
IRequestHandler::~IRequestHandler()
{
}

/*
 * Handles a GET request. Defaults to 405 Method Not Allowed.
 */
bool IRequestHandler::handleGet(const HttpRequest &request,
		HttpResponse &response)
{
	(void)request;
	response.setStatusCode(405);
	return (true);
}

/*
 * Handles a POST request. Defaults to 405 Method Not Allowed.
 */
bool IRequestHandler::handlePost(const HttpRequest &request,
		HttpResponse &response)
{
	(void)request;
	response.setStatusCode(405);
	return (true);
}

/*
 * Handles a DELETE request. Defaults to 405 Method Not Allowed.
 */
bool IRequestHandler::handleDelete(const HttpRequest &request,
		HttpResponse &response)
{
	(void)request;
	response.setStatusCode(405);
	return (true);
}

/*
 * Dispatches the request to the method matching its HTTP verb.
 * Unsupported verbs yield 405 Method Not Allowed.
 */
bool IRequestHandler::handle(const HttpRequest &request,
		HttpResponse &response)
{
	const std::string	&method = request.getMethod();

	if (method == "GET")
		return (handleGet(request, response));
	if (method == "POST")
		return (handlePost(request, response));
	if (method == "DELETE")
		return (handleDelete(request, response));
	response.setStatusCode(405);
	return (true);
}

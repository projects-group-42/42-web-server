/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpResponse.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/12 13:36:33 by jucoelho          #+#    #+#             */
/*   Updated: 2026/07/11 20:01:50 by jucoelho         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

# include "http/HttpResponse.hpp"
# include "utils/Utils.hpp"
# include <iostream>
# include <sstream>
# include <ctime>

/**
 * @brief Default constructor for the HttpResponse class.
 *
 * Initializes a standard HTTP response structure, setting the default protocol 
 * version to "HTTP/1.1" and the initial status code to 200 (OK).
 */
HttpResponse::HttpResponse(void)
	: _version("HTTP/1.1"), _status_code(200)
{
}

/**
 * @brief Copy constructor for the HttpResponse class.
 *
 * Creates a new HttpResponse instance by performing a deep copy of all fields 
 * from an existing response entity.
 *
 * @param copy Constant reference to the source response object to replicate.
 */
HttpResponse::HttpResponse(const HttpResponse &copy)
{
	*this = copy;
}

/**
 * @brief Copy assignment operator for the HttpResponse class.
 *
 * Copies the protocol version, headers map, body content, and numeric status code 
 * from another response instance into this one, preventing self-assignment.
 *
 * @param other Constant reference to the object to assign from.
 * @return Reference to the current instance (*this).
 */
HttpResponse& HttpResponse::operator=(const HttpResponse &other)
{
	if (this != &other)
	{
		_version = other._version;
		_headers = other._headers;
		_body = other._body;
		_status_code = other._status_code;
	}
	return (*this);
}

/**
 * @brief Destructor for the HttpResponse class.
 *
 * Cleans up memory and structures allocated by the response entity.
 */
HttpResponse::~HttpResponse(void)
{
}

/**
 * @brief Retrieves the HTTP protocol version of the response.
 *
 * @return Constant reference to the version string (e.g., "HTTP/1.1").
 */
const std::string& HttpResponse::getVersion(void) const
{
	return (_version);
}

/**
 * @brief Retrieves the numeric HTTP status code of the response.
 *
 * @return The integer status code value (e.g., 200, 404, 500).
 */
int HttpResponse::getStatusCode(void) const
{
	return (_status_code);
}

/**
 * @brief Retrieves the complete internal map containing all assigned response headers.
 *
 * @return Constant reference to the std::map holding lowercase header keys and their values.
 */
const std::map<std::string, std::string>& HttpResponse::getHeaders(void) const
{
	return (_headers);
}

/**
 * @brief Looks up and retrieves the value of a specific response header field.
 *
 * Transforms the requested lookup key to lowercase to ensure a case-insensitive match, 
 * aligning with HTTP protocol requirements.
 *
 * @param key The name of the header field to locate (e.g., "Content-Type", "server").
 * @return The corresponding header value string if found, or an empty string otherwise.
 */
const std::string HttpResponse::getHeaderValue(const std::string &key) const
{
	std::map<std::string, std::string>::const_iterator it = _headers.find(toLower(key));
	if (it != _headers.end())
		return (it->second);
	return ("");
}

/**
 * @brief Retrieves the raw body payload data associated with this response.
 *
 * @return Constant reference to the body payload string block.
 */
const std::string& HttpResponse::getBody(void) const
{
	return (_body);
}

/**
 * @brief Sets the HTTP protocol version string for the response.
 *
 * @param version String defining the protocol version token.
 */
void	HttpResponse::setVersion(const std::string &version)
{
	_version = version;
}

/**
 * @brief Sets the numeric HTTP status code for the response.
 *
 * @param status The integer code establishing the response status (e.g., 200, 403).
 */
void	HttpResponse::setStatusCode(int status)
{
	_status_code = status;
}

/**
 * @brief Stores or updates a specific header key-value pair inside the response.
 *
 * Enforces uniform, case-insensitive header mapping by converting the descriptor 
 * key to lowercase before inserting it into the internal container.
 *
 * @param key The descriptor name label of the response header line.
 * @param value The value data string assigned to the header key property.
 */
void	HttpResponse::setHeaders(const std::string &key, const std::string &value)
{
	_headers[toLower(key)] = value;
}

/**
 * @brief Sets the raw payload data representing the message body component of the response.
 *
 * @param body String containing data bytes, HTML markup, or raw file sequences to transmit.
 */
void	HttpResponse::setBody(const std::string &body)
{
	_body = body;
}

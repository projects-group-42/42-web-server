/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpRequest.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/11 22:21:02 by dajesus-          #+#    #+#             */
/*   Updated: 2026/07/11 19:53:24 by jucoelho         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

# include "http/HttpRequest.hpp"
# include "utils/Utils.hpp"

/**
 * @brief Default constructor for the HttpRequest class.
 *
 * Initializes an empty HTTP request object with unpopulated fields.
 */
HttpRequest::HttpRequest(void)
{
}

/**
 * @brief Copy constructor for the HttpRequest class.
 *
 * Creates a new HttpRequest instance by performing a deep copy of all fields 
 * from the source request entity.
 *
 * @param copy Constant reference to the source object to replicate.
 */
HttpRequest::HttpRequest(const HttpRequest &copy)
{
	*this = copy;
}

/**
 * @brief Copy assignment operator for the HttpRequest class.
 *
 * Copies the state and contents (method, URI, query, version, headers, and body) 
 * from another request instance into this one, preventing self-assignment.
 *
 * @param other Constant reference to the object to assign from.
 * @return Reference to the current instance (*this).
 */
HttpRequest& HttpRequest::operator=(const HttpRequest &other)
{
	if (this != &other)
	{
		_method = other._method;
		_uri = other._uri;
		_query = other._query;
		_version = other._version;
		_headers = other._headers;
		_body = other._body;
	}
	return (*this);
}

/**
 * @brief Destructor for the HttpRequest class.
 *
 * Cleans up memory and resources allocated by the request entity.
 */
HttpRequest::~HttpRequest(void)
{
}

/**
 * @brief Retrieves the HTTP method of the request.
 *
 * @return Constant reference to the method string (e.g., "GET", "POST", "DELETE").
 */
const std::string& HttpRequest::getMethod(void) const
{
	return (_method);
}

/**
 * @brief Retrieves the requested Uniform Resource Identifier (URI) path.
 *
 * @return Constant reference to the sanitized URI string resource path.
 */
const std::string& HttpRequest::getUri(void) const
{
	return (_uri);
}

/**
 * @brief Retrieves the raw or decoded URL query string.
 *
 * @return Constant reference to the query parameter component trailing after the `?`.
 */
const std::string& HttpRequest::getQuery(void) const
{
	return (_query);
}

/**
 * @brief Retrieves the HTTP protocol version string.
 *
 * @return Constant reference to the protocol specification token (e.g., "HTTP/1.1").
 */
const std::string& HttpRequest::getVersion(void) const
{
	return (_version);
}

/**
 * @brief Retrieves the entire internal map collection containing all HTTP request headers.
 *
 * @return Constant reference to the std::map holding lowercase header keys and their values.
 */
const std::map<std::string, std::string>& HttpRequest::getHeaders(void) const
{
	return (_headers);
}

/**
 * @brief Looks up and retrieves the value of a specific HTTP header key.
 *
 * Converts the requested lookup key to lowercase to ensure a case-insensitive match, 
 * matching the RFC compliance threshold.
 *
 * @param key The name of the header field to find (e.g., "Content-Length", "host").
 * @return The corresponding header value string if matched, or an empty string if not found.
 */
const std::string HttpRequest::getHeaderValue(const std::string &key) const
{
	std::map<std::string, std::string>::const_iterator it = _headers.find(toLower(key));
	if (it != _headers.end())
		return (it->second);
	return ("");
}

/**
 * @brief Retrieves the raw body payload data contained within the HTTP request.
 *
 * @return Constant reference to the body payload string.
 */
const std::string& HttpRequest::getBody(void) const
{
	return (_body);
}

/**
 * @brief Sets the HTTP request method token.
 *
 * @param method String slice token denoting the requested action verb.
 */
void	HttpRequest::setMethod(const std::string &method)
{
	_method = method;
}

/**
 * @brief Sets the resource path URI target.
 *
 * @param uri String defining the targeted resource endpoint block.
 */
void	HttpRequest::setUri(const std::string &uri)
{
	_uri = uri;
}

/**
 * @brief Sets the parsed query parameters string component.
 *
 * @param query String block representing key-value parameters extracted from the URI.
 */
void	HttpRequest::setQuery(const std::string &query)
{
	_query = query;
}

/**
 * @brief Sets the HTTP protocol specification standard version.
 *
 * @param version String establishing the protocol version (e.g., "HTTP/1.1").
 */
void	HttpRequest::setVersion(const std::string &version)
{
	_version = version;
}

/**
 * @brief Stores or overrides a specific header key-value property.
 *
 * Transforms the storage lookup key token to lower case explicitly to enforce 
 * uniform case-insensitive storage constraints inside the underlying std::map container.
 *
 * @param key The descriptor label of the incoming header field line.
 * @param value The value payload assigned to the incoming header property block.
 */
void	HttpRequest::setHeaders(const std::string &key, const std::string &value)
{
	_headers[toLower(key)] = value;
}

/**
 * @brief Sets the raw data stream payload representing the body component of the HTTP request.
 *
 * @param body String chunk containing incoming POST, PUT or upload content sequences.
 */
void	HttpRequest::setBody(const std::string &body)
{
	_body = body;
}

/**
 * @brief Evaluates whether a particular header element exists within the current request instance.
 *
 * Transforms the tested configuration identifier string to lowercase to ensure 
 * accurate identification regardless of client formatting variations.
 *
 * @param key The targeted search header field name token.
 * @return true if the transformed key maps to an existing configuration row, false otherwise.
 */
bool	HttpRequest::hasHeader(const std::string &key) const
{
	return (_headers.find(toLower(key)) != _headers.end());
}



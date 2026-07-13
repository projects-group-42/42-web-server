/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ResponseBuilder.cpp                                :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/12 13:36:33 by jucoelho          #+#    #+#             */
/*   Updated: 2026/07/11 20:09:30 by jucoelho         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

# include "http/ResponseBuilder.hpp"
# include <ctime>
# include <sstream>
#include <map>
#include <string>

/**
 * @brief Default constructor for the ResponseBuilder class.
 * Initializes the response builder with a default server identification 
 * name string ("Webserv/1.0: PorFavorFuncione") and sets the connection 
 * persistence flag (keep-alive) to false by default.
 */
ResponseBuilder::ResponseBuilder(void)
	: _server_name("Webserv/1.0: PorFavorFuncione"), _keep_alive(false)
{
}

/**
 * @brief Parameterized constructor for the ResponseBuilder class.
 * Configures the builder with a customized server identification name and 
 * sets the connection state behavior configuration.
 * @param serverName The custom identification string sent in the "Server" header field.
 * @param keepAlive Boolean flag establishing whether connections should persist (true) or close (false).
 */
ResponseBuilder::ResponseBuilder(const std::string &serverName, bool keepAlive)
	: _server_name(serverName), _keep_alive(keepAlive)
{
}

/**
 * @brief Copy constructor for the ResponseBuilder class.
 * Replicates an existing ResponseBuilder state instance via copy assignment.
 * @param copy Constant reference to the source builder object to copy from.
 */
ResponseBuilder::ResponseBuilder(const ResponseBuilder &copy)
{
	*this = copy;
}

/**
 * @brief Copy assignment operator for the ResponseBuilder class.
 * Deep copies the configuration fields (server name and keep-alive flag) from another 
 * instance into this one, ensuring distinct memory and preventing self-assignment.
 * @param other Constant reference to the object to copy from.
 * @return Reference to the current instance (*this).
 */
ResponseBuilder& ResponseBuilder::operator=(const ResponseBuilder &other)
{
	if (this != &other)
	{
		_server_name = other._server_name;
		_keep_alive = other._keep_alive;
	}
	return (*this);
}

/**
 * @brief Destructor for the ResponseBuilder class.
 * Cleans up allocated resources upon builder object destruction.
 */
ResponseBuilder::~ResponseBuilder(void)
{
}

/**
 * @brief Maps an HTTP integer status code to its canonical Reason-Phrase string.
 * Provides standard human-readable descriptions for successful, redirection, client error, 
 * and server error response codes as specified by HTTP protocols.
 * @param status The integer HTTP status code to look up (e.g., 200, 404, 500).
 * @return The canonical status phrase string, or "Unknown Status" if undefined.
 */
std::string ResponseBuilder::getStatusMessage(int status) const
{
	switch(status)
	{
		case 200: return "OK";
		case 201: return "Created";
		case 204: return "No Content";
		case 301: return "Moved Permanently";
		case 302: return "Found";
		case 400: return "Bad Request";
		case 401: return "Unauthorized";
		case 403: return "Forbidden";
		case 404: return "Not Found";
		case 405: return "Method Not Allowed";
		case 408: return "Request Timeout";
		case 411: return "Length Required";
		case 413: return "Content Too Large";
		case 414: return "URI Too Long";
		case 500: return "Internal Server Error";
		case 501: return "Not Implemented";
		case 502: return "Bad Gateway";
		case 503: return "Service Unavailable";
		case 504: return "Gateway Timeout";
		case 505: return "HTTP Version Not Supported";
		default:  return "Unknown Status";
	}
}

/**
 * @brief Generates the initial HTTP Status-Line row string.
 *
 * Concatenates the matching client protocol version text, the numeric status code value, 
 * and the appropriate text phrase explanation ended with a strict CRLF sequence.
 * @param request Constant reference to the triggering incoming HttpRequest instance.
 * @param response Constant reference to the populated target HttpResponse instance.
 * @return The complete constructed status line row string (e.g., "HTTP/1.1 200 OK\r\n").
 */
std::string ResponseBuilder::getStatusLine(
	const HttpRequest &request, const HttpResponse &response) const
{
	std::string status_line;
	std::ostringstream str_status;
	str_status << response.getStatusCode();

	status_line.append(request.getVersion());
	status_line.append(" ");
	status_line.append(str_status.str());
	status_line.append(" ");
	status_line.append(getStatusMessage(response.getStatusCode()));
	status_line.append("\r\n");
	return (status_line);
}

/**
 * @brief Automatically injects the core default HTTP header rows into an HttpResponse instance.
 * Computes and injects mandatory compliance fields required by the HTTP/1.1 specification, 
 * including the system timestamp clock ("date"), server identification token ("server"), 
 * accurate payload length metrics ("content-length"), and connection lifecycle state controls ("connection").
 * @param response Reference to the target HttpResponse object being loaded.
 */
void ResponseBuilder::setDefaultHeaders(HttpResponse &response) const
{
	std::ostringstream	b_size;
	b_size << response.getBody().size();
	
	response.setHeaders("date", getHttpDate());
	response.setHeaders("server", _server_name);
	response.setHeaders("content-length", b_size.str());
	response.setHeaders("connection", _keep_alive ? "keep-alive" : "close");
}

/**
 * @brief Central pipeline orchestrating the complete string assembly of an HTTP raw response payload.
 *
 * Triggers fallback error template page generation if the current response lacks body contents. 
 * Resolves default protocol header requirements, strings together the leading Status-Line, appends 
 * header map elements formatted as "Key: Value\r\n", inserts the crucial separating blank line, 
 * and finishes by attaching raw trailing content body bytes.
 * @param request Constant reference to the context HttpRequest source entity.
 * @param response Reference to the target HttpResponse object entity being compiled.
 * @return The fully compiled raw HTTP string payload ready to transmit across network sockets.
 */
std::string ResponseBuilder::builder(
	const HttpRequest &request, HttpResponse &response)
{
	std::string result;

	if (response.getBody().empty())
		response.setBody(ErrorPage(response.getStatusCode()));
	setDefaultHeaders(response);
	result.append(getStatusLine(request, response));
	std::map<std::string, std::string>::const_iterator it;
	for (it = response.getHeaders().begin();
		it != response.getHeaders().end(); ++it)
	{
		result.append(it->first);
		result.append(": ");
		result.append(it->second);
		result.append("\r\n");
	}
	result.append("\r\n");
	result.append(response.getBody());
	return (result);
}

/**
 * @brief Fallback handler generating raw HTML mock-up pages for error responses.
 * Produces standard inline boilerplate web pages capturing error numerical metadata titles 
 * alongside textual messages when a specialized configuration directory page definition 
 * is completely missing.
 * @param status The integer status error identifier code.
 * @return A string block representing raw readable standard HTML browser structures.
 */
std::string ResponseBuilder::ErrorPage(int status) const
{
	std::ostringstream str_error;
	std::string str_html;

	str_error << status;
	str_html.append("<html><body><h1>");
	str_html.append(str_error.str());
	str_html.append(" ");
	str_html.append(getStatusMessage(status));
	str_html.append("</h1></body></html>");
	return (str_html);
}

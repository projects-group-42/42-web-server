/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ResponseBuilder.cpp                                :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/12 13:36:33 by jucoelho          #+#    #+#             */
/*   Updated: 2026/06/29 18:29:23 by jucoelho         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

# include "http/ResponseBuilder.hpp"
# include <ctime>
# include <sstream>
#include <map>
#include <string>

ResponseBuilder::ResponseBuilder(void)
	: _server_name("Webserv/1.0: PorFavorFuncione"), _keep_alive(false)
{
}

ResponseBuilder::ResponseBuilder(const std::string &serverName, bool keepAlive)
	: _server_name(serverName), _keep_alive(keepAlive)
{
}

ResponseBuilder::ResponseBuilder(const ResponseBuilder &copy)
{
	*this = copy;
}

ResponseBuilder& ResponseBuilder::operator=(const ResponseBuilder &other)
{
	if (this != &other)
	{
		_server_name = other._server_name;
		_keep_alive = other._keep_alive;
	}
	return (*this);
}

ResponseBuilder::~ResponseBuilder(void)
{
}

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

std::string ResponseBuilder::whatTimeIsIt(void) const
{
	char		buffer[100];
	time_t		now = time(NULL);
	struct tm*	timeinfo = gmtime(&now);

	strftime(buffer, sizeof(buffer), "%a, %d %b %Y %H:%M:%S GMT", timeinfo);
	return (std::string(buffer));
}

void ResponseBuilder::setDefaultHeaders(HttpResponse &response) const
{
	std::ostringstream	b_size;
	b_size << response.getBody().size();
	
	response.setHeaders("date", whatTimeIsIt());
	response.setHeaders("server", _server_name);
	response.setHeaders("content-length", b_size.str());
	response.setHeaders("connection", _keep_alive ? "keep-alive" : "close");
}

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

/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpResponse.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/12 13:36:33 by jucoelho          #+#    #+#             */
/*   Updated: 2026/06/24 21:14:45 by jucoelho         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

# include "http/HttpResponse.hpp"
# include "utils/Utils.hpp"
# include <iostream>
# include <sstream>
# include <ctime>

HttpResponse::HttpResponse(void)
	: _version("HTTP/1.1"), _status(200)
{
}

HttpResponse::HttpResponse(const HttpResponse &copy)
{
	*this = copy;
}

HttpResponse& HttpResponse::operator=(const HttpResponse &other)
{
	if (this != &other)
	{
		_version = other._version;
		_headers = other._headers;
		_body = other._body;
		_status = other._status;
	}
	return (*this);
}

HttpResponse::~HttpResponse(void)
{
}

const std::string& HttpResponse::getVersion(void) const
{
	return (_version);
}

const std::map<std::string, std::string>& HttpResponse::getHeaders(void) const
{
	return (_headers);
}

const std::string& HttpResponse::getBody(void) const
{
	return (_body);
}

int HttpResponse::getStatus(void) const
{
	return (_status);
}

void	HttpResponse::setStatus(int status)
{
	_status = status;
}

void	HttpResponse::setVersion(const std::string &version)
{
	_version = version;
}

void	HttpResponse::addHeader(const std::string &key, const std::string &value)
{
	_headers[toLower(key)] = value;
}

void	HttpResponse::setBody(const std::string &body)
{
	_body = body;
}

std::string HttpResponse::getHeader(const std::string &key) const
{
	std::map<std::string, std::string>::const_iterator it = _headers.find(toLower(key));
	if (it != _headers.end())
		return (it->second);
	return ("");
}

std::string HttpResponse::getStatusMessage(void) const
{
	switch(_status)
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

std::string HttpResponse::whatTimeIsIt(void)
{
	time_t now = time(NULL);
	struct tm* timeinfo = gmtime(&now);
	char buffer[100];
	strftime(buffer, sizeof(buffer), "%a, %d %b %Y %H:%M:%S GMT", timeinfo);
	return (buffer);
}
void HttpResponse::setDefaultHeaders(void)
{
	std::string date = whatTimeIsIt();
	_headers["date"] = date;

	_headers["server"] = "Webserv/1.0: Por favor funcione";
	
	std::ostringstream oss;
	oss << _body.size();
	_headers["content-length"] = oss.str();

	_headers["connection"] = "close";
}

std::string HttpResponse::getStatusLine(void) const
{
	std::string status_line;
	std::ostringstream oss;
	oss << _status;

	status_line.append(oss.str());
	status_line.append(" ");
	status_line.append(getStatusMessage());
	status_line.append("\r\n");
	return (status_line);
}

std::string HttpResponse::responseBuilder(void) const
{
	//tá aqui só para o SetDefauçtHeaders não se sentir sozinho,
	//quando tiver um handle a gente muda para lá kkkkk
	const_cast<HttpResponse*>(this)->setDefaultHeaders();
	std::string response;
	std::map <std::string, std::string>::const_iterator it;
	
	response.append(_version);
	response.append(" ");
	response.append(getStatusLine());
	
	for (it = _headers.begin(); it != _headers.end(); ++it)
	{
		response.append(it->first);
		response.append(": ");
		response.append(it->second);
		response.append("\r\n");
	}
	response.append("\r\n");
	response.append(_body);
	return (response);
}


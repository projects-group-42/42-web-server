/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   RequestParser.cpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/11 22:21:02 by dajesus-          #+#    #+#             */
/*   Updated: 2026/06/23 19:12:04 by jucoelho         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

# include "http/RequestParser.hpp"
# include <iostream>

RequestParser::RequestParser(void)
	: _buffer(""), _len(0), _psr_state(REQUEST_LINE)
{
	
}
RequestParser::RequestParser(std::string buffer, ssize_t len)
	: _buffer(buffer), _len(len), _psr_state(REQUEST_LINE)
{
	
}

RequestParser::RequestParser(const RequestParser &copy)
{
	*this = copy;
}

RequestParser& RequestParser::operator=(const RequestParser &other)
{
	if (this != &other)
	{
		_buffer = other._buffer;
		_len = other._len;
		_psr_state = other._psr_state;
	}
	return (*this);
}

RequestParser::~RequestParser(void)
{
}

t_psr_state RequestParser::get_psr_state(void) const
{
	return (_psr_state);
}

const HttpRequest& RequestParser::getRequest(void) const
{
	return (_request);
}

std::string RequestParser::str_extract(std::string str_find, int nbr)
{
	size_t pos = _buffer.find(str_find);
	if (pos == std::string::npos)
		return ("");
	std::string temp = _buffer.substr(0, pos);
	_buffer.erase(0, pos + nbr);
	return (temp);
}
bool RequestParser::prs_body(void)
{
	size_t pos = strtoul(_request.getHeader("Content-Length").c_str(), NULL, 10);
	if (pos == 0)
	{
		_psr_state = COMPLETE;
		return true;
	}
	size_t b_size = _buffer.size();
	if (pos > b_size)
		return false;
	if (pos == 0)
		return false;;
	std::string temp = _buffer.substr(0, pos);
	_request.setBody(temp);
	_buffer.erase(0, pos);
	_psr_state = COMPLETE;
	return true;
}

bool RequestParser::prs_headers(void)
{
	while (_buffer.find(":") != std::string::npos)
	{
		std::string str_key = str_extract(":", 1);
		if (_buffer[0] == ' ')
			_buffer.erase(0, 1);
		std::string str_value = str_extract("\r\n", 2);
		_request.addHeader(str_key, str_value);
	}
	std::string str_value = str_extract("\r\n", 2);
	if (!_request.hasHeader("Host"))
	{
		std::cout << "Error" << std::endl;
		_psr_state = ERROR;
		return false;
	}
	return true;
}

bool RequestParser::prs_method(void)
{
	std::string str_method = str_extract(" ", 1);
	if (str_method == "")
		return false;
	_request.setMethod(str_method);

	std::string str_uri = str_extract(" ", 1);
	if (str_uri == "")
		return false;
	size_t pos = str_uri.find("?");
	if (pos == std::string::npos)
		_request.setQuery("");
	else
	{
		std::string str_query = str_uri.substr(pos + 1);
		str_uri.erase(pos);
		_request.setQuery(str_query);
	}
	_request.setUri(str_uri);
	
	std::string str_version = str_extract("\r\n", 2);
	if (str_version == "") 
		return false;
	_request.setVersion(str_version);
	return true;
}

void RequestParser::feed(const char *buffer, ssize_t bytes_read)
{
	_buffer.append(buffer, bytes_read);
	while (_buffer.size() > 0 && (_buffer[0] == '\r' || _buffer[0] == '\n'))
		_buffer.erase(0, 1);
	if (_psr_state == REQUEST_LINE || _psr_state == HEADERS)
	{
		size_t pos = _buffer.find("\r\n\r\n");
		if (pos == std::string::npos)
		{
			return;
		}
		if(!prs_method())
		{
			std::cout << "Error" << std::endl;
			_psr_state = ERROR;
			return;
		}
		_psr_state = HEADERS;
		if(!prs_headers())
		{
			std::cout << "Error" << std::endl;
			_psr_state = ERROR;
			return;
		}
		_psr_state = BODY;
	}
	if (_psr_state == BODY)
	{
		if(!prs_body())
		{
			return;
		}
		_psr_state = COMPLETE;
	}
}


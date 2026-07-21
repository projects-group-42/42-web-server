/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   RequestParser.cpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/11 22:21:02 by dajesus-          #+#    #+#             */
/*   Updated: 2026/07/19 16:24:41 by jucoelho         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

# include "http/RequestParser.hpp"
# include "utils/Logger.hpp"
# include <iostream>
# include <cctype>

RequestParser::RequestParser(void)
	: _buffer(""), _len(0), _psr_state(REQUEST_LINE), _error_code(0)
{
}
RequestParser::RequestParser(std::string buffer, ssize_t len)
	: _buffer(buffer), _len(len), _psr_state(REQUEST_LINE), _error_code(0)
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
		_request = other._request;
		_error_code = other._error_code;
	}
	return (*this);
}

RequestParser::~RequestParser(void)
{
}

/*
 * Prepares the parser to read the next request on a reused connection.
 * Clears the previous request and error state, rewinds to the request
 * line, and re-parses any bytes already buffered from a pipelined request.
 */
void RequestParser::reset(void)
{
	_request = HttpRequest();
	_psr_state = REQUEST_LINE;
	_error_code = 0;
	if (!_buffer.empty())
		feed("", 0);
}

t_psr_state RequestParser::get_psr_state(void) const
{
	return (_psr_state);
}

const HttpRequest& RequestParser::getRequest(void) const
{
	return (_request);
}

void RequestParser::setErrorState(int status_code)
{
	_error_code = status_code;
	_psr_state = ERROR;
}

int RequestParser::get_error_code(void) const
{
	return (_error_code);
}

bool RequestParser::isValidVersion(const std::string &version) const
{
	if (version.length() < 8)
		return (false);
	if (version.substr(0, 5) != "HTTP/")
		return (false);
	size_t dot = version.find('.', 5);
	if (dot == std::string::npos || dot == 5)
		return (false);
	for (size_t i = 5; i < dot; i++)
	{
		if (!std::isdigit(version[i]))
			return (false);
	}
	for (size_t i = dot + 1; i < version.length(); i++)
	{
		if (!std::isdigit(version[i]))
			return (false);
	}
	return (true);
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

bool RequestParser::prs_chunked_size(void)
{
	size_t pos = _buffer.find("\r\n");
	if (pos == std::string::npos)
		return true;
	std::string line = _buffer.substr(0, pos);
	size_t semi = line.find(';');
	if (semi != std::string::npos)
		line.erase(semi);
	if (line.empty()
		|| line.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos)
	{
		Logger::error("400 Bad Request");
		setErrorState(400);
		return true;
	}
	_chunk_size = strtoul(line.c_str(), NULL, 16);
	_buffer.erase(0, pos + 2);
	if (_chunk_size == 0)
		_psr_state = CHUNK_TRAILER;
	else
		_psr_state = CHUNK_DATA;
	return false;
}

bool RequestParser::prs_chunked_data(void)
{
	if (_buffer.size() < _chunk_size + 2)
		return true;
	if (_buffer.compare(_chunk_size, 2, "\r\n") != 0)
	{
		Logger::error("400 Bad Request");
		setErrorState(400);
		return true;
	}
	_request.setBody(_buffer.substr(0, _chunk_size));
	_buffer.erase(0, _chunk_size + 2);
	_psr_state = CHUNK_SIZE;
	return false;
}

bool RequestParser::prs_chunked_trailer(void)
{
	if (_buffer.size() < 2)
		return true;
	if (_buffer.compare(0, 2, "\r\n") == 0)
	{
		_buffer.erase(0, 2);
		_psr_state = COMPLETE;
		return true;
	}
	size_t pos = _buffer.find("\r\n");
	if (pos == std::string::npos)
		return true;
	_buffer.erase(0, pos + 2);
	return false;
}

bool RequestParser::prs_body(void)
{
	//h_size é p tamanho do body declarado no header
	size_t h_size = strtoul(_request.getHeaderValue("Content-Length").c_str(), NULL, 10);
	if (h_size == 0)
	{
		_psr_state = COMPLETE;
		return true;
	}
	//b_size é o tamanho do body
	size_t b_size = _buffer.size();
	//se o tamnho declarado no header for maior que o do buffer retorna falso e le de novo
	if (h_size > b_size)
		return false;
	//temp lê do buffer até o tamanho do header
	std::string temp = _buffer.substr(0, h_size);
	_request.setBody(temp);
	_buffer.erase(0, h_size);
	_psr_state = COMPLETE;
	return true;
}

bool RequestParser::prs_headers(void)
{
	if (_buffer.size() > MAX_HEADER_SIZE)
	{
		setErrorState(431);
		return false;
	}
	while (_buffer.find(":") != std::string::npos)
	{
		std::string str_key = str_extract(":", 1);
		if (str_key.empty() || str_key.find(" ") != std::string::npos)
		{
			setErrorState(400);
			return (false);
		}
		if (_buffer[0] == ' ')
			_buffer.erase(0, 1);
		std::string str_value = str_extract("\r\n", 2);
		if (_buffer[0] == ' ')
			_buffer.erase(0, 1);
		_request.setHeaders(str_key, str_value);
	}
	std::string str_value = str_extract("\r\n", 2);
	if (!_request.hasHeader("Host"))
	{
		Logger::error("Request missing Host header");
		setErrorState(400);
		return (false);
	}
	return true;
}

std::string RequestParser::percent_decoding(std::string str)
{
	std::string hex = "0123456789abcdefABCDEF";
	std::string result;

	for (size_t pos = 0; pos < str.size(); pos++)
	{
		if (str[pos] != '%')
			result += str[pos];
		else
		{
			if (pos + 2 >= str.size())
				return ("");//malformadoRejeitar sequências malformadas com 400
			if (hex.find(str[pos + 1]) == std::string::npos 
				|| hex.find(str[pos + 2]) == std::string::npos)
				return ("");
			else
			{
				if (str[pos + 1] == '0' && str[pos + 2] == '0')
					return ("");//rejeitar dangerous reserved
				if (str[pos + 1] == '2' && (
					str[pos + 2] == 'F' || str[pos + 2] == 'f'))
					return ("");//rejeitar dangerous reserved
				else
				{
					std::string str_hex;
					str_hex += str[pos + 1];
					str_hex += str[pos + 2];
					char decoded = (char)strtol(str_hex.c_str(), NULL, 16);
					result += decoded;
					pos += 2;
					
				}
			}
		}
	}
	return (result);
}

bool RequestParser::prs_method(void)
{
	std::string str_method = str_extract(" ", 1);
	if (str_method == "")
	{
		setErrorState(400);
		return false;
	}

	_request.setMethod(str_method);
	std::string str_uri = str_extract(" ", 1);
	if (str_uri == "")
	{
		setErrorState(400);
		return false;
	}
	if (str_uri.size() > MAX_URI_LENGTH)
	{
		setErrorState(414);
		return false;
	}
	size_t pos = str_uri.find("?");
	if (pos == std::string::npos)
		_request.setQuery("");
	else
	{
		std::string str_query = str_uri.substr(pos + 1);
		str_uri.erase(pos);
		if (str_query.find("%") == std::string::npos)
			_request.setQuery(str_query);
		else
		{
			std::string d_query = percent_decoding(str_query);
			if (d_query.empty())
			{
				setErrorState(400);
				return false;
			}
			_request.setQuery(d_query);
		}
	}
	if (str_uri.find("%") == std::string::npos)
			_request.setUri(str_uri);
		else
		{
			std::string d_uri = percent_decoding(str_uri);
			if (d_uri.empty())
			{
				setErrorState(400);
				return false;
			}
			if (d_uri.size() > MAX_URI_LENGTH)
			{
				setErrorState(414);
				return false;
			}
			_request.setUri(d_uri);
		}
	
	std::string str_version = str_extract("\r\n", 2);
	if (str_version == "") 
	{
		setErrorState(400);
		return false;
	}
	if (!isValidVersion(str_version))
	{
		setErrorState(400);
		return false;
	}
	_request.setVersion(str_version);
	return true;
}

void RequestParser::feed(const char *buffer, ssize_t bytes_read)
{
	_buffer.append(buffer, bytes_read);
	if (_psr_state == REQUEST_LINE)
	{
		while (_buffer.size() > 0 && (_buffer[0] == '\r' || _buffer[0] == '\n'))
			_buffer.erase(0, 1);
	}
	if (_psr_state == REQUEST_LINE || _psr_state == HEADERS)
	{
		size_t pos = _buffer.find("\r\n\r\n");
		if (pos == std::string::npos)
		{
			return;
		}
		if(!prs_method())
		{
			Logger::error("RequestParser: invalid request line");
			_psr_state = ERROR;
			return;
		}
		_psr_state = HEADERS;
		if(!prs_headers())
		{
			Logger::error("RequestParser: invalid headers");
			_psr_state = ERROR;
			return;
		}
		if (_request.hasHeader("Transfer-Encoding") &&
			_request.hasHeader("Content-Length"))
		{
			Logger::error("Bad Request");
			setErrorState(400);
			return;
		}
		if (_request.hasHeader("Transfer-Encoding"))
			_psr_state = CHUNK_SIZE;
		else
			_psr_state = BODY;
	}
	while (_psr_state == CHUNK_SIZE || _psr_state == CHUNK_DATA
		|| _psr_state == CHUNK_TRAILER)
	{
		bool stop;
		if (_psr_state == CHUNK_SIZE)
			stop = prs_chunked_size();
		else if (_psr_state == CHUNK_DATA)
			stop = prs_chunked_data();
		else
			stop = prs_chunked_trailer();
		if (stop)
			return;
	}
	if (_psr_state == BODY)
	{
		prs_body();
		return;
	}
}

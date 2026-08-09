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
# include "utils/Utils.hpp"
# include <iostream>
# include <cctype>
# include <limits>

RequestParser::RequestParser(void)
	: _buffer(""), _len(0), _psr_state(REQUEST_LINE), _error_code(0),
	  _chunk_size(0), _content_length(0), _chunked_total(0),
	  _max_body_size(DEFAULT_MAX_BODY_SIZE)
{
}
RequestParser::RequestParser(std::string buffer, ssize_t len)
	: _buffer(buffer), _len(len), _psr_state(REQUEST_LINE), _error_code(0),
	  _chunk_size(0), _content_length(0), _chunked_total(0),
	  _max_body_size(DEFAULT_MAX_BODY_SIZE)
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
		_chunk_size = other._chunk_size;
		_content_length = other._content_length;
		_chunked_total = other._chunked_total;
		_max_body_size = other._max_body_size;
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
	_chunk_size = 0;
	_content_length = 0;
	_chunked_total = 0;
	if (!_buffer.empty())
		feed("", 0);
}

/*
 * Sets the largest body this parser accepts before answering 413. The value is
 * the widest client_max_body_size configured for the port the connection was
 * accepted on, since the server block and location serving the request are
 * only known once its headers have been read. A negative value lifts the limit.
 */
void RequestParser::setMaxBodySize(long maxBodySize)
{
	_max_body_size = maxBodySize;
}

/*
 * Reports whether a body of `size` bytes is over the configured limit.
 */
bool RequestParser::bodyLimitExceeded(size_t size) const
{
	return (_max_body_size >= 0
		&& size > static_cast<size_t>(_max_body_size));
}

/*
 * Caps how many bytes may pile up in _buffer while the request is still
 * incomplete, so a client cannot exhaust the memory of the process by never
 * terminating what it sends. Until the header block is closed the cap is a
 * whole request line on top of MAX_HEADER_SIZE, which bounds a header block
 * that never gets its "\r\n\r\n" while leaving prs_headers to own the exact
 * 431 threshold: the buffer still holds the request line here, so a tighter
 * cap would reject a legal request only when it arrives split. Afterwards the
 * cap is the configured body limit plus one header block of slack, which
 * covers the bytes of a pipelined request read along with the body.
 */
bool RequestParser::checkBufferLimit(void)
{
	if (_psr_state == REQUEST_LINE || _psr_state == HEADERS)
	{
		if (_buffer.size() > MAX_REQUEST_LINE_SIZE + MAX_HEADER_SIZE
			&& _buffer.find("\r\n\r\n") == std::string::npos)
		{
			Logger::error("431 Request Header Fields Too Large");
			setErrorState(431);
			return (false);
		}
		return (true);
	}
	if (_max_body_size >= 0
		&& _buffer.size() > static_cast<size_t>(_max_body_size)
			+ MAX_HEADER_SIZE)
	{
		Logger::error("413 Content Too Large");
		setErrorState(413);
		return (false);
	}
	return (true);
}

/*
 * Converts the declared Content-Length into the byte count the body parser
 * reads. The value is accumulated digit by digit and abandoned as soon as it
 * passes the configured limit, so a length far too large to be honoured is
 * refused before a single body byte is buffered, and no overflow can turn it
 * into a small number. The surrounding spaces and tabs a client may pad a
 * header value with are dropped first, so only what they wrap has to be
 * digits. Anything that is not a plain decimal number, "-1" included, is
 * malformed rather than oversized and answers 400.
 */
bool RequestParser::parseContentLength(void)
{
	std::string	raw = _request.getHeaderValue("Content-Length");
	size_t		widest = std::numeric_limits<size_t>::max();
	size_t		start = raw.find_first_not_of(" \t");
	size_t		end = raw.find_last_not_of(" \t");

	_content_length = 0;
	if (start == std::string::npos)
		return (true);
	raw = raw.substr(start, end - start + 1);
	if (raw.find_first_not_of("0123456789") != std::string::npos)
	{
		Logger::error("400 Bad Request");
		setErrorState(400);
		return (false);
	}
	for (size_t i = 0; i < raw.size(); i++)
	{
		size_t	digit = static_cast<size_t>(raw[i] - '0');

		if (_content_length > (widest - digit) / 10)
		{
			Logger::error("413 Content Too Large");
			setErrorState(413);
			return (false);
		}
		_content_length = _content_length * 10 + digit;
		if (bodyLimitExceeded(_content_length))
		{
			Logger::error("413 Content Too Large");
			setErrorState(413);
			return (false);
		}
	}
	return (true);
}

t_psr_state RequestParser::get_psr_state(void) const
{
	return (_psr_state);
}

const HttpRequest& RequestParser::getRequest(void) const
{
	return (_request);
}

/*
 * Fails the request with the given status. Whatever is left in _buffer is
 * dropped: once a request is malformed the byte stream can no longer be split
 * into requests the way the client meant it, so the remainder is not a
 * pipelined request the server may answer. Keeping it would let reset() parse
 * it on the next turn of a kept-alive connection and dispatch bytes the client
 * smuggled inside the request that was just refused.
 */
void RequestParser::setErrorState(int status_code)
{
	_error_code = status_code;
	_psr_state = ERROR;
	_buffer.clear();
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
	if (bodyLimitExceeded(_chunk_size)
		|| bodyLimitExceeded(_chunked_total + _chunk_size))
	{
		Logger::error("413 Content Too Large");
		setErrorState(413);
		return true;
	}
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
	_chunked_total += _chunk_size;
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

/*
 * Reads the body of a request that declared a Content-Length. The length was
 * validated against the configured limit when the headers were parsed, so this
 * only waits for the declared bytes to arrive and hands them to the request.
 */
bool RequestParser::prs_body(void)
{
	if (_content_length == 0)
	{
		_psr_state = COMPLETE;
		return true;
	}
	if (_content_length > _buffer.size())
		return false;
	_request.setBody(_buffer.substr(0, _content_length));
	_buffer.erase(0, _content_length);
	_psr_state = COMPLETE;
	return true;
}

/*
 * Reads the header block into the request. Headers are stored in a map keyed by
 * their lowercased name, so a repeated Content-Length would silently overwrite
 * the first one and leave the server reading a body of a length the client may
 * not have meant. RFC 7230 3.3.3 requires the request to be refused instead:
 * the second Content-Length is caught here, while the field values are still
 * separate, and answers 400.
 */
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
		if (!_buffer.empty() && _buffer[0] == ' ')
			_buffer.erase(0, 1);
		std::string str_value = str_extract("\r\n", 2);
		if (!_buffer.empty() && _buffer[0] == ' ')
			_buffer.erase(0, 1);
		if (toLower(str_key) == "content-length"
			&& _request.hasHeader("Content-Length"))
		{
			Logger::error("400 Bad Request: duplicated Content-Length");
			setErrorState(400);
			return (false);
		}
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
	/*
	 * The version is well formed but names a protocol this server does not
	 * speak. RFC 9110 answers that with 505 rather than a generic 400.
	 */
	if (str_version != "HTTP/1.0" && str_version != "HTTP/1.1")
	{
		Logger::error("505 HTTP Version Not Supported");
		setErrorState(505);
		return false;
	}
	_request.setVersion(str_version);
	return true;
}

void RequestParser::feed(const char *buffer, ssize_t bytes_read)
{
	_buffer.append(buffer, bytes_read);
	if (!checkBufferLimit())
		return;
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
		{
			size_t headers_end = _buffer.find("\r\n\r\n");
			if (headers_end == std::string::npos)
			{
				Logger::error("RequestParser: missing header terminator");
				setErrorState(400);
				return;
			}
			std::string body_data = _buffer.substr(headers_end + 4);
			_buffer.erase(headers_end + 2);
			if(!prs_headers())
			{
				Logger::error("RequestParser: invalid headers");
				_psr_state = ERROR;
				return;
			}
			_buffer = body_data;
		}
		if (_request.hasHeader("Transfer-Encoding") &&
			_request.hasHeader("Content-Length"))
		{
			Logger::error("Bad Request");
			setErrorState(400);
			return;
		}
		/*
		 * Only "chunked" is understood. Any other transfer coding is refused
		 * with 501 instead of being fed to the chunk parser, which used to
		 * blame the client for a malformed chunk size it never sent. The value
		 * is trimmed of the optional whitespace RFC 9112 allows around it, so a
		 * legal "chunked" carrying a trailing space or tab is not mistaken for
		 * an unsupported coding.
		 */
		if (_request.hasHeader("Transfer-Encoding"))
		{
			std::string	coding = toLower(_request.getHeaderValue(
							"Transfer-Encoding"));
			std::string::size_type	first = coding.find_first_not_of(" \t\r");
			std::string::size_type	last = coding.find_last_not_of(" \t\r");

			if (first == std::string::npos)
				coding = "";
			else
				coding = coding.substr(first, last - first + 1);
			if (coding != "chunked")
			{
				Logger::error("501 Not Implemented");
				setErrorState(501);
				return;
			}
		}
		if (_request.hasHeader("Transfer-Encoding"))
			_psr_state = CHUNK_SIZE;
		else
		{
			if (!parseContentLength())
			{
				Logger::error("RequestParser: invalid Content-Length");
				return;
			}
			_psr_state = BODY;
		}
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

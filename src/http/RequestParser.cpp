/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   RequestParser.cpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/11 22:21:02 by dajesus-          #+#    #+#             */
/*   Updated: 2026/07/11 19:46:11 by jucoelho         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

# include "http/RequestParser.hpp"
# include "utils/Logger.hpp"
# include <iostream>

/**
 * @brief Default constructor for the RequestParser class.
 * * Initializes the parser with an empty inner buffer, zero-length tracker, 
 * and sets the initial state machine position to REQUEST_LINE.
 */
RequestParser::RequestParser(void)
	: _buffer(""), _len(0), _psr_state(REQUEST_LINE)
{
	
}

/**
 * @brief Parameterized constructor for the RequestParser class.
 * * Pre-loads the internal buffer with initial incoming data and sets 
 * the initial parser state to REQUEST_LINE.
 * * @param buffer Initial raw string data received from the connection.
 * @param len The length of the initial chunk of data.
 */
RequestParser::RequestParser(std::string buffer, ssize_t len)
	: _buffer(buffer), _len(len), _psr_state(REQUEST_LINE)
{
	
}

/**
 * @brief Copy constructor for the RequestParser class.
 * * Creates a new parser instance by deep copying the state, internal buffers, 
 * and processed request information from an existing instance.
 * * @param copy Constant reference to the source parser object.
 */
RequestParser::RequestParser(const RequestParser &copy)
{
	*this = copy;
}

/**
 * @brief Copy assignment operator for the RequestParser class.
 * * Assigns the internal buffer, length, and state machine position 
 * from another parser instance to this one, ensuring distinct memory.
 * * @param other Constant reference to the object to be assigned.
 * @return Reference to the current instance (*this).
 */
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

/**
 * @brief Destructor for the RequestParser class.
 * * Cleans up parser resources upon object destruction.
 */
RequestParser::~RequestParser(void)
{
}

/**
 * @brief Retrieves the current state of the parser's internal state machine.
 * * @return The current t_psr_state enum value (e.g., REQUEST_LINE, HEADERS, BODY, COMPLETE, ERROR).
 */
t_psr_state RequestParser::get_psr_state(void) const
{
	return (_psr_state);
}

/**
 * @brief Retrieves a constant reference to the populated HttpRequest object.
 * * @return Constant reference to the internal HttpRequest instance containing parsed fields.
 */
const HttpRequest& RequestParser::getRequest(void) const
{
	return (_request);
}

/**
 * @brief Extracts a substring up to a delimiter and updates the main parser buffer.
 * * Finds the first occurrence of a specific string delimiter, crops everything preceding it 
 * as the extracted token, and permanently removes both the token and the delimiter 
 * from the front of the internal `_buffer`.
 * * @param str_find The token or sequence delimiter to look for (e.g., " ", "\r\n").
 * @param nbr The total length of the delimiter to safely erase from the front alongside the token.
 * @return The extracted string block up to the delimiter, or an empty string if not found.
 */
std::string RequestParser::str_extract(std::string str_find, int nbr)
{
	size_t pos = _buffer.find(str_find);
	if (pos == std::string::npos)
		return ("");
	std::string temp = _buffer.substr(0, pos);
	_buffer.erase(0, pos + nbr);
	return (temp);
}

/**
 * @brief Parses the HTTP Request Body using the expected Content-Length.
 * * Checks the extracted Content-Length header. If it is 0, the parsing completes immediately. 
 * Otherwise, it validates whether enough bytes have arrived in the internal buffer. 
 * If the buffer contains the full expected length, it loads it into the HttpRequest object 
 * and advances the state to COMPLETE.
 * * @return true if body parsing is completely finished (or skipped), false if the parser 
 * needs to wait for more incoming data fragments.
 */
bool RequestParser::prs_body(void)
{
	size_t h_size = strtoul(_request.getHeaderValue("Content-Length").c_str(), NULL, 10);
	if (h_size == 0)
	{
		_psr_state = COMPLETE;
		return true;
	}
	size_t b_size = _buffer.size();
	if (h_size > b_size)
		return false;
	if (h_size == 0)
		return false;
	std::string temp = _buffer.substr(0, h_size);
	_request.setBody(temp);
	_buffer.erase(0, h_size);
	_psr_state = COMPLETE;
	return true;
}

/**
 * @brief Parses individual HTTP header rows from the internal buffer.
 * * Iterates through key-value pairs separated by colons. It validates headers formatting 
 * (e.g., ensuring no illegal spaces inside the header keys), strips leading spaces from values, 
 * and stores them in the HttpRequest entity. Finally, verifies that the mandatory 
 * "Host" header is present to comply with HTTP/1.1 specifications.
 * * @return true if all found headers were successfully parsed and valid, false if a structural 
 * or semantic validation rule was violated (triggers an ERROR state).
 */
bool RequestParser::prs_headers(void)
{
	while (_buffer.find(":") != std::string::npos)
	{
		std::string str_key = str_extract(":", 1);
		if (str_key.find(" ") != std::string::npos)
		{
			std::cout << "Error" << std::endl;
			_psr_state = ERROR;
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
		_psr_state = ERROR;
		return (false);
	}
	return true;
}

/**
 * @brief Decodes a percent-encoded (%HH) URI/Query component string.
 * * Converts hexadecimal representations back to standard characters (e.g., "%20" -> " ").
 * Contains security validations to block malformed inputs or malicious inputs.
 * * @note Security guards explicitly reject dangerous reserved sequences like "%00" (Null Byte Injection) 
 * or "%2F" (encoded forward slashes used to attempt hidden Directory Traversal attacks).
 * * @param str The raw encoded string slice to process.
 * @return The fully decoded plain string, or an empty string if a dangerous or malformed sequence is caught.
 */
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

/**
 * @brief Parses the initial HTTP Request-Line containing the Method, URI, and Version.
 * * Extracts space-separated tokens from the first row. It isolates the HTTP Method, 
 * separates the URI from potential raw Query parameters (`?`), executes percent-decoding 
 * for safe resource routing on both components, and validates the protocol string version.
 * * @return true if the full Request-Line string structure was accurately populated, 
 * false if formatting was illegal or token extractions failed.
 */
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
		if (str_query.find("%") == std::string::npos)
			_request.setQuery(str_query);
		else
		{
			std::string d_query = percent_decoding(str_query);
			if (d_query.empty())
				return false;
			_request.setQuery(d_query);
		}
	}
	if (str_uri.find("%") == std::string::npos)
			_request.setUri(str_uri);
		else
		{
			std::string d_uri = percent_decoding(str_uri);
			if (d_uri.empty())
				return false;
			_request.setUri(d_uri);
		}
	
	std::string str_version = str_extract("\r\n", 2);
	if (str_version == "") 
		return false;
	_request.setVersion(str_version);
	return true;
}

/**
 * @brief Appends new raw chunks of network bytes and executes the parser state-machine.
 * * This is the central input pipeline invoked by the server event loop. It buffers 
 * sequential socket data, handles non-blocking fragmentation by maintaining states, 
 * skips trailing junk whitespace, and drives the extraction pipeline from initial header block 
 * down through body payloads to complete state.
 * * @param buffer Pointer to the character array reading chunk inputs.
 * @param bytes_read Count of bytes fetched in the current read iteration.
 */
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

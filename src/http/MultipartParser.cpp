/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   MultipartParser.cpp                                :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dajesus- <dajesus-@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/21 23:17:44 by dajesus-          #+#    #+#             */
/*   Updated: 2026/07/24 18:21:30 by dajesus-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

# include "http/MultipartParser.hpp"
# include "utils/Utils.hpp"

bool MultipartPart::isFile(void) const
{
	return (!filename.empty());
}

MultipartParser::MultipartParser(void)
	: _errorCode(0)
{
}

MultipartParser::~MultipartParser(void)
{
}

const std::vector<MultipartPart> &MultipartParser::getParts(void) const
{
	return (_parts);
}

int MultipartParser::getErrorCode(void) const
{
	return (_errorCode);
}

std::string MultipartParser::trim(const std::string &str)
{
	size_t	start = 0;
	size_t	end = str.size();

	while (start < end && (str[start] == ' ' || str[start] == '\t'))
		++start;
	while (end > start && (str[end - 1] == ' ' || str[end - 1] == '\t'
			|| str[end - 1] == '\r' || str[end - 1] == '\n'))
		--end;
	return (str.substr(start, end - start));
}

std::string MultipartParser::extractParam(const std::string &headerValue,
		const std::string &param)
{
	std::string	needle = toLower(param) + "=";
	std::string	lower = toLower(headerValue);
	size_t		pos = lower.find(needle);

	while (pos != std::string::npos)
	{
		if (pos == 0 || headerValue[pos - 1] == ';' || headerValue[pos - 1] == ' ')
			break;
		pos = lower.find(needle, pos + 1);
	}
	if (pos == std::string::npos)
		return ("");

	size_t	start = pos + needle.size();
	if (start >= headerValue.size())
		return ("");
	if (headerValue[start] == '"')
	{
		size_t	end = headerValue.find('"', start + 1);
		if (end == std::string::npos)
			return ("");
		return (headerValue.substr(start + 1, end - start - 1));
	}
	size_t		end = headerValue.find(';', start);
	std::string	value = (end == std::string::npos)
		? headerValue.substr(start) : headerValue.substr(start, end - start);
	return (trim(value));
}

bool MultipartParser::extractBoundary(const std::string &contentType,
		std::string &boundary)
{
	std::string	lower = toLower(contentType);
	size_t		pos = lower.find("boundary=");

	boundary.clear();
	if (pos == std::string::npos)
		return (false);

	size_t	start = pos + 9; // strlen("boundary=")
	if (start >= contentType.size())
		return (false);

	if (contentType[start] == '"')
	{
		size_t	end = contentType.find('"', start + 1);
		if (end == std::string::npos)
			return (false);
		boundary = contentType.substr(start + 1, end - start - 1);
	}
	else
	{
		size_t	end = contentType.find(';', start);
		boundary = (end == std::string::npos)
			? contentType.substr(start) : contentType.substr(start, end - start);
		boundary = trim(boundary);
	}
	return (!boundary.empty());
}

bool MultipartParser::parseHeaders(const std::string &block, MultipartPart &part)
{
	size_t	pos = 0;

	while (pos <= block.size())
	{
		size_t		lineEnd = block.find("\r\n", pos);
		std::string	line = (lineEnd == std::string::npos)
			? block.substr(pos) : block.substr(pos, lineEnd - pos);

		if (!line.empty())
		{
			size_t	colon = line.find(':');
			if (colon == std::string::npos)
				return (false);
			std::string	key = toLower(trim(line.substr(0, colon)));
			std::string	value = trim(line.substr(colon + 1));
			if (key.empty())
				return (false);
			part.headers[key] = value;
		}
		if (lineEnd == std::string::npos)
			break;
		pos = lineEnd + 2;
	}

	std::map<std::string, std::string>::const_iterator it =
		part.headers.find("content-disposition");
	if (it == part.headers.end())
		return (false);
	part.name = extractParam(it->second, "name");
	part.filename = extractParam(it->second, "filename");
	return (true);
}

bool MultipartParser::parse(const std::string &body, const std::string &boundary)
{
	_parts.clear();
	_errorCode = 0;
	if (boundary.empty())
	{
		_errorCode = 400;
		return (false);
	}

	std::string	delimiter = "--" + boundary;
	size_t		pos = body.find(delimiter);
	if (pos == std::string::npos)
	{
		_errorCode = 400;
		return (false);
	}
	pos += delimiter.size();

	while (true)
	{
		if (pos + 2 > body.size())
		{
			_errorCode = 400;
			return (false);
		}
		if (body.compare(pos, 2, "--") == 0)
			return (true);
		if (body.compare(pos, 2, "\r\n") != 0)
		{
			_errorCode = 400;
			return (false);
		}
		pos += 2;

		size_t	nextDelim = body.find("\r\n" + delimiter, pos);
		if (nextDelim == std::string::npos)
		{
			_errorCode = 400;
			return (false);
		}

		std::string	partBlock = body.substr(pos, nextDelim - pos);
		size_t		sep = partBlock.find("\r\n\r\n");
		if (sep == std::string::npos)
		{
			_errorCode = 400;
			return (false);
		}

		MultipartPart	part;
		if (!parseHeaders(partBlock.substr(0, sep), part))
		{
			_errorCode = 400;
			return (false);
		}
		part.content = partBlock.substr(sep + 4);
		_parts.push_back(part);

		pos = nextDelim + 2 + delimiter.size();
	}
}

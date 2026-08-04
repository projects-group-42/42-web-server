/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Utils.cpp                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dajesus- <dajesus-@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/01 17:05:24 by jucoelho          #+#    #+#             */
/*   Updated: 2026/06/29 21:26:44 by dajesus-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

# include "utils/Utils.hpp"
# include <stdexcept>
# include <fcntl.h>
# include <cctype>
# include <iostream>

//F_SETFL = Set file(FD) flags | GET -> get flags
void setNonBlocking(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1)
		throw std::runtime_error("fcntl(F_GETFL) fail");
	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
		throw std::runtime_error("fcntl(F_SETFL, O_NONBLOCK) fail");
}

std::string toLower(const std::string &str)
{
	std::string result = str;
	for (size_t i = 0; i < result.size(); i++)
		result[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(result[i])));
	return result;
}

std::string getHttpDate(void)
{
	time_t		now = time(NULL);
	struct tm	*gmt = std::gmtime(&now);
	char		buf[100];
	std::strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S GMT", gmt);
	return (std::string(buf));
}

/**
 * @brief Replaces every character that carries meaning in HTML by its entity.
 * Must be applied to any attacker-controlled text before it is written into a
 * generated page, including inside attribute values.
 * @param str The raw text.
 * @return The text with '&', '<', '>', '"' and '\'' escaped.
 */
std::string htmlEscape(const std::string &str)
{
	std::string	result;

	for (size_t i = 0; i < str.size(); i++)
	{
		if (str[i] == '&')
			result += "&amp;";
		else if (str[i] == '<')
			result += "&lt;";
		else if (str[i] == '>')
			result += "&gt;";
		else if (str[i] == '"')
			result += "&quot;";
		else if (str[i] == '\'')
			result += "&#39;";
		else
			result += str[i];
	}
	return (result);
}

/**
 * @brief Percent-encodes every byte that is not allowed unescaped in a URI.
 * @param str The text to encode.
 * @param keepSlash Whether '/' is left as a path separator.
 * @return The percent-encoded text.
 */
static std::string	urlEncode(const std::string &str, bool keepSlash)
{
	static const char	*hex = "0123456789ABCDEF";
	std::string			result;

	for (size_t i = 0; i < str.size(); i++)
	{
		unsigned char	c = static_cast<unsigned char>(str[i]);

		if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~'
			|| (c == '/' && keepSlash))
			result += static_cast<char>(c);
		else
		{
			result += '%';
			result += hex[c >> 4];
			result += hex[c & 0x0F];
		}
	}
	return (result);
}

/**
 * @brief Percent-encodes a URI path, preserving '/' as a separator.
 * @param str The path to encode.
 * @return The percent-encoded path.
 */
std::string urlEncodePath(const std::string &str)
{
	return (urlEncode(str, true));
}

/**
 * @brief Percent-encodes a single path segment, escaping '/' as well.
 * Used for file names so that no name can inject an extra path component.
 * @param str The segment to encode.
 * @return The percent-encoded segment.
 */
std::string urlEncodeSegment(const std::string &str)
{
	return (urlEncode(str, false));
}
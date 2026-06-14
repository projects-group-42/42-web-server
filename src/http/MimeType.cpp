/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   MimeType.cpp                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dajesus- <dajesus-@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/12 19:55:24 by dajesus-          #+#    #+#             */
/*   Updated: 2026/06/12 22:30:20 by dajesus-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

# include "http/MimeType.hpp"
# include "utils/Utils.hpp"
# include <map>

static std::map<std::string, std::string> buildMap(void)
{
	std::map<std::string, std::string> m;
	m["html"] = "text/html";
	m["htm"]  = "text/html";
	m["css"]  = "text/css";
	m["js"]   = "application/javascript";
	m["json"] = "application/json";
	m["xml"]  = "application/xml";
	m["png"]  = "image/png";
	m["jpg"]  = "image/jpeg";
	m["jpeg"] = "image/jpeg";
	m["gif"]  = "image/gif";
	m["svg"]  = "image/svg+xml";
	m["ico"]  = "image/x-icon";
	m["webp"] = "image/webp";
	m["txt"]  = "text/plain";
	m["csv"]  = "text/csv";
	m["md"]   = "text/markdown";
	m["pdf"]  = "application/pdf";
	return (m);
}

std::string	mimeType_resolve(const std::string &filePath)
{
	static const std::map<std::string, std::string> map = buildMap();
	const std::string::size_type dotPos = filePath.rfind('.');

	if (dotPos == std::string::npos || dotPos == 0 || dotPos + 1 == filePath.size())
		return ("application/octet-stream");
	std::string ext = toLower(filePath.substr(dotPos + 1));
	std::map<std::string, std::string>::const_iterator it = map.find(ext);
	if (it != map.end())
		return (it->second);
	return ("application/octet-stream");
}

/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   MimeType.cpp                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dajesus- <dajesus-@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/12 19:55:24 by dajesus-          #+#    #+#             */
/*   Updated: 2026/06/12 21:08:05 by dajesus-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

# include "http/MimeType.hpp"

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
	return ("application/octet-stream");
}

/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ServerConfig.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/20 19:37:04 by dajesus-          #+#    #+#             */
/*   Updated: 2026/08/01 12:31:54 by jucoelho         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

# include "config/ServerConfig.hpp"

LocationConfig::LocationConfig(void)
	: path(""), root(""), index(""), autoindex(false), allowedMethods(),
	  uploadStore(""), cgiPass(), returnCode(0), returnUrl(""),
	  clientMaxBodySize(-1)
{
}

LocationConfig::LocationConfig(const std::string &path):
	path(path), 
	root(""), 
	index(""), 
	autoindex(false), 
	allowedMethods(),
	uploadStore(""),
	cgiPass(),
	returnCode(0),
	returnUrl(""),
	clientMaxBodySize(-1)
{
}

ServerConfig::ServerConfig(void):
	host("0.0.0.0"), 
	port(8080),
	serverNames(), 
	root(""),
	index("index.html"),
	autoindex(false),
	errorPages(),
	clientMaxBodySize(1 * 1024 * 1024),
	locations()
{
}

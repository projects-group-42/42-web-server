/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ServerConfig.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dajesus- <dajesus-@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/20 19:37:04 by dajesus-          #+#    #+#             */
/*   Updated: 2026/07/20 23:14:02 by dajesus-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

# include "config/ServerConfig.hpp"

LocationConfig::LocationConfig(void)
	: path(""), root(""), index(""), autoindex(false), allowedMethods(),
	  uploadStore(""), cgiPass(""), returnCode(0), returnUrl(""),
	  clientMaxBodySize(-1)
{
}

LocationConfig::LocationConfig(const std::string &path)
	: path(path), root(""), index(""), autoindex(false), allowedMethods(),
	  uploadStore(""), cgiPass(""), returnCode(0), returnUrl(""),
	  clientMaxBodySize(-1)
{
}

ServerConfig::ServerConfig(void)
	: host("0.0.0.0"), port(8080), serverNames(), root(""),
	  index("index.html"), errorPages(), clientMaxBodySize(1 * 1024 * 1024),
	  locations()
{
}

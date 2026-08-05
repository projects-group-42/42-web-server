/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ServerConfig.hpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dajesus- <dajesus-@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/20 19:31:52 by dajesus-          #+#    #+#             */
/*   Updated: 2026/07/20 23:12:19 by dajesus-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef SERVERCONFIG_HPP
# define SERVERCONFIG_HPP

# include <string>
# include <vector>
# include <map>

# define DEFAULT_ROOT	"www"

struct LocationConfig
{
	std::string					path;
	std::string					root;
	std::string					index;
	bool						autoindex;
	std::vector<std::string>	allowedMethods;
	std::string					uploadStore;
	std::map<std::string, std::string>	cgiPass;
	int							returnCode;
	std::string					returnUrl;
	long						clientMaxBodySize;
	LocationConfig(void);
	LocationConfig(const std::string &path);
};

struct ServerConfig
{
	std::string					host;
	int							port;
	std::vector<std::string>	serverNames;
	std::string					root;
	std::string					index;
	bool						autoindex;
	std::map<int, std::string>	errorPages;
	long						clientMaxBodySize;
	std::vector<LocationConfig>	locations;
	ServerConfig(void);
};

#endif

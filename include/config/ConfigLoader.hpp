/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ConfigLoader.hpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/18 11:21:16 by dajesus-          #+#    #+#             */
/*   Updated: 2026/08/07 01:29:29 by galves-a         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CONFIGLOADER_HPP
# define CONFIGLOADER_HPP

# include <cstdlib>
# include <fstream>
# include <map>
# include <sstream>
# include <string>
# include <vector>
# include "config/Lexer.hpp"
# include "config/ConfigParser.hpp"
# include "config/ConfigAST.hpp"
# include "config/ServerConfig.hpp"

class ConfigLoader
{
	private:
		std::string					_file_path;
		std::vector<ServerConfig>	_servers;
		ConfigBlock					_tree;

	public:
		ConfigLoader(void);
		ConfigLoader(const std::string &file_path);
		ConfigLoader(const ConfigLoader &copy);
		ConfigLoader &operator=(const ConfigLoader &other);
		~ConfigLoader(void);

		std::vector<ServerConfig> loader(void);

		static int			parsePort(const std::string &token);
		static std::string	parseHost(const std::string &token);

	private:
		std::string	configPath(void);
		void		parse_directives(const ConfigBlock &block,
					ServerConfig &server);
		void		parse_locations(const ConfigBlock &block,
					ServerConfig &server);
		void		applyListen(const std::vector<std::string> &args,
					ServerConfig &server);
		void		parse_names(const ConfigDirective &directive,
					ServerConfig &server);

		static std::string	parseRoot(const ConfigDirective &d);
		static long			parseBodySize(const ConfigDirective &d);
		static bool			parseBool(const std::string &s);
		static void			parseAutoindex(bool &autoindex,
							const ConfigDirective &d);
		static void			parseIndex(std::string &index,
							const ConfigDirective &d);
		static int			parseErrorCode(const std::string &token);
		static void			parseErrorPage(
							std::map<int, std::string> &pages,
							const ConfigDirective &d);
		static int			parseRedirectCode(const std::string &token);
		static void			parseReturn(LocationConfig &loc,
							const ConfigDirective &d);
		static void			parseCgiPass(
							std::map<std::string, std::string> &interpreters,
							const ConfigDirective &d);
		static void			parseLimitExcept(
							std::vector<std::string> &methods,
							const ConfigDirective &d);
};

#endif

/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ConfigLoader.hpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: galves-a <galves-a@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/18 11:21:16 by dajesus-          #+#    #+#             */
/*   Updated: 2026/08/03 10:24:11 by galves-a         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CONFIGLOADER_HPP
# define CONFIGLOADER_HPP

# include <cstdlib>
# include <fstream>
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
		ServerConfig				_config;
		ConfigBlock					_tree;

	public:
		ConfigLoader(void);
		ConfigLoader(const std::string &file_path);
		ConfigLoader(const ConfigLoader &copy);
		ConfigLoader &operator=(const ConfigLoader &other);
		~ConfigLoader(void);

		ServerConfig loader(void);

		static int			parsePort(const std::string &token);
		static std::string	parseHost(const std::string &token);

	private:
		std::string configPath(void);
		void parse_listen(void);
		void parse_server_names(void);
		void parse_locations(void);
		void applyListen(const std::vector<std::string> &args);
		void applyServerName(const std::vector<std::string> &args);

		static bool	parseBool(const std::string &s);
		static void	parseAutoindex(LocationConfig &loc,
					const ConfigDirective &d);
};

#endif

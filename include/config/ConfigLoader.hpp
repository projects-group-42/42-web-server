/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ConfigLoader.hpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/18 11:21:16 by dajesus-          #+#    #+#             */
/*   Updated: 2026/08/03 12:21:15 by jucoelho         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CONFIGLOADER_HPP
# define CONFIGLOADER_HPP

# include <cstdlib>
# include <fstream>
# include <sstream>
# include <string>
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

	private:
		std::string		configPath(void);
		void			parse_listen(const ConfigDirective &directive, ServerConfig &server);
		void			parse_names(const ConfigDirective &directive, ServerConfig &server);
		void			parse_root(const ConfigDirective &directive, ServerConfig &server);
		void			parse_index(const ConfigDirective &directive, ServerConfig &server);
		void			parse_error_page(const ConfigDirective &directive, ServerConfig &server);
		LocationConfig	parse_location(const ConfigBlock &block);
		void			parse_client_max_body_size(const ConfigDirective &directive, ServerConfig &server);
};

#endif

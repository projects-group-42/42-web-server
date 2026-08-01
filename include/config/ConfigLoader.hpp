/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ConfigLoader1.hpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/18 11:21:16 by dajesus-          #+#    #+#             */
/*   Updated: 2026/08/01 17:15:26 by jucoelho         ###   ########.fr       */
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
		ServerConfig				_config;
		ConfigBlock					_tree;

	public:
		ConfigLoader(void);
		ConfigLoader(const std::string &file_path);
		ConfigLoader(const ConfigLoader &copy);
		ConfigLoader &operator=(const ConfigLoader &other);
		~ConfigLoader(void);

		ServerConfig loader(void);

	private:
		std::string configPath(void);
		void parse_listen(void);
};

#endif

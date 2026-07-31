/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ConfigUtils.hpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/18 11:21:16 by dajesus-          #+#    #+#             */
/*   Updated: 2026/07/30 18:31:46 by jucoelho         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CONFIGUTILS_HPP
# define CONFIGUTILS_HPP

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include "config/Lexer.hpp"
#include "config/ConfigParser.hpp"
# include "config/ConfigAST.hpp"
# include "config/ConfigAST.hpp"

struct HandleConfig
{
	std::string					_file_path;

	HandleConfig(void);
	HandleConfig(const std::string &file_path);
	std::string configPath(void);
	ConfigBlock handle(void);
};

#endif



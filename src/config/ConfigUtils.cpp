/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ConfigUtils.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/18 11:25:05 by dajesus-          #+#    #+#             */
/*   Updated: 2026/07/31 18:49:59 by jucoelho         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

# include "config/ConfigUtils.hpp"

HandleConfig::HandleConfig(void) : _file_path("simple.conf")
{
}

HandleConfig::HandleConfig(const std::string &file_path)
	: _file_path(file_path)
{
}

std::string HandleConfig::configPath(void)
{
	std::ifstream file(_file_path.c_str());
	std::stringstream buffer;

	if (!file.is_open())
	{
		return ("");
	}
	buffer << file.rdbuf();
	return (buffer.str());
}

ConfigBlock HandleConfig::handle(void)
{
	Lexer lexer(configPath());
	std::vector<Token> tokens = lexer.tokenize();
	ConfigParser parser(tokens);
	ConfigBlock tree = parser.parse();
	return (tree);
}


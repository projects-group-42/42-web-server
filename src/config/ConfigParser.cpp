/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ConfigParser.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dajesus- <dajesus-@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/17 11:46:22 by dajesus-          #+#    #+#             */
/*   Updated: 2026/07/17 13:59:13 by dajesus-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

# include "config/ConfigParser.hpp"

# include <stdexcept>
# include <sstream>

ConfigParser::ConfigParser(void) : _tokens(), _pos(0)
{
}

ConfigParser::ConfigParser(const std::vector<Token> &tokens)
	: _tokens(tokens), _pos(0)
{
}

ConfigParser::ConfigParser(const ConfigParser &copy)
	: _tokens(copy._tokens), _pos(copy._pos)
{
}

ConfigParser &ConfigParser::operator=(const ConfigParser &other)
{
	if (this != &other)
	{
		_tokens = other._tokens;
		_pos = other._pos;
	}
	return (*this);
}

ConfigParser::~ConfigParser(void)
{
}

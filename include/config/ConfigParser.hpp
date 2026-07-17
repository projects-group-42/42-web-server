/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ConfigParser.hpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dajesus- <dajesus-@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/17 11:43:14 by dajesus-          #+#    #+#             */
/*   Updated: 2026/07/17 13:57:31 by dajesus-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CONFIGPARSER_HPP
# define CONFIGPARSER_HPP

# include "config/Lexer.hpp"

class ConfigParser
{
	private:
		std::vector<Token>	_tokens;
		size_t				_pos;

	public:
		ConfigParser(void);
		ConfigParser(const std::vector<Token> &tokens);
		ConfigParser(const ConfigParser &copy);
		ConfigParser &operator=(const ConfigParser &other);
		~ConfigParser(void);
};

#endif

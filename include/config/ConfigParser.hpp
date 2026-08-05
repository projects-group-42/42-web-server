/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ConfigParser.hpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/17 11:43:14 by dajesus-          #+#    #+#             */
/*   Updated: 2026/08/01 13:35:10 by jucoelho         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CONFIGPARSER_HPP
# define CONFIGPARSER_HPP

# include <string>
# include <vector>

# include "config/Lexer.hpp"
# include "config/ConfigAST.hpp"

class ConfigParser
{
	private:
		std::vector<Token>	_tokens;
		size_t				_pos;

		const Token	&peek(void) const;
		const Token	&advance(void);
		bool		check(t_token_type type) const;
		const Token	&expect(t_token_type type, const std::string &expected);

		void	parseEntry(ConfigBlock &parent);
		void	parseBlockBody(ConfigBlock &block);

	public:
		ConfigParser(void);
		ConfigParser(const std::vector<Token> &tokens);
		ConfigParser(const ConfigParser &copy);
		ConfigParser &operator=(const ConfigParser &other);
		~ConfigParser(void);

		ConfigBlock	parse(void);

};

#endif

/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ConfigParser.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dajesus- <dajesus-@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/17 11:46:22 by dajesus-          #+#    #+#             */
/*   Updated: 2026/07/19 16:10:11 by dajesus-         ###   ########.fr       */
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

const Token &ConfigParser::peek(void) const
{
	return (_tokens[_pos]);
}

const Token &ConfigParser::advance(void)
{
	const Token &tok = _tokens[_pos];

	if (tok.type != TOKEN_EOF)
		_pos++;
	return (tok);
}

bool ConfigParser::check(t_token_type type) const
{
	return (peek().type == type);
}

const Token &ConfigParser::expect(t_token_type type, const std::string &expected)
{
	if (!check(type))
	{
		std::ostringstream oss;

		oss << "Config error at line " << peek().line << ", column "
			<< peek().column << ": expected " << expected;
		throw std::runtime_error(oss.str());
	}
	return (advance());
}

void ConfigParser::parseEntry(ConfigBlock &parent)
{
	if (!check(TOKEN_WORD))
	{
		std::ostringstream oss;

		oss << "Config error at line " << peek().line << ", column "
			<< peek().column << ": expected directive or block name";
		throw std::runtime_error(oss.str());
	}

	Token					nameToken = advance();
	std::vector<std::string>	args;

	while (check(TOKEN_WORD))
		args.push_back(advance().value);

	if (check(TOKEN_LBRACE))
	{
		advance();

		ConfigBlock	block(nameToken.value, nameToken.line, nameToken.column);

		block.args = args;
		parseBlockBody(block);
		expect(TOKEN_RBRACE, "'}' to close block '" + nameToken.value + "'");
		parent.children.push_back(block);
		return ;
	}
	if (check(TOKEN_SEMICOLON))
	{
		advance();

		ConfigDirective	directive(nameToken.value, nameToken.line, nameToken.column);

		directive.args = args;
		parent.directives.push_back(directive);
		return ;
	}

	std::ostringstream oss;

	oss << "Config error at line " << peek().line << ", column "
		<< peek().column << ": expected '{' or ';' after '"
		<< nameToken.value << "'";
	throw std::runtime_error(oss.str());
}

void ConfigParser::parseBlockBody(ConfigBlock &block)
{
	while (!check(TOKEN_RBRACE) && !check(TOKEN_EOF))
		parseEntry(block);
}

ConfigBlock ConfigParser::parse(void)
{
	if (_tokens.empty())
		throw std::runtime_error("Config error: empty token stream");

	ConfigBlock	root("config", 0, 0);

	parseBlockBody(root);
	expect(TOKEN_EOF, "end of file");
	return (root);
}

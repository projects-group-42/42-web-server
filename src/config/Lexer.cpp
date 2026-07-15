/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Lexer.cpp                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dajesus- <dajesus-@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/12 20:27:44 by dajesus-          #+#    #+#             */
/*   Updated: 2026/07/13 23:05:38 by dajesus-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

# include "config/Lexer.hpp"
# include <cctype>

/* ------------------------------------------------------------------ */
/* Token                                                               */
/* ------------------------------------------------------------------ */

Token::Token(void) : type(TOKEN_EOF), value(""), line(0), column(0)
{
}

Token::Token(t_token_type type, const std::string &value, int line, int column)
	: type(type), value(value), line(line), column(column)
{
}

/* ------------------------------------------------------------------ */
/* Lexer - canonical form                                              */
/* ------------------------------------------------------------------ */

Lexer::Lexer(void) : _source(""), _pos(0), _line(1), _column(1)
{
}

Lexer::Lexer(const std::string &source)
	: _source(source), _pos(0), _line(1), _column(1)
{
}

Lexer::Lexer(const Lexer &copy)
	: _source(copy._source), _pos(copy._pos),
	  _line(copy._line), _column(copy._column)
{
}

Lexer& Lexer::operator=(const Lexer &other)
{
	if (this != &other)
	{
		_source = other._source;
		_pos = other._pos;
		_line = other._line;
		_column = other._column;
	}
	return (*this);
}

Lexer::~Lexer(void)
{
}

/* ------------------------------------------------------------------ */
/* Cursor helpers                                                       */
/* ------------------------------------------------------------------ */

bool Lexer::isAtEnd(void) const
{
	return (_pos >= _source.size());
}

char Lexer::peek(void) const
{
	if (isAtEnd())
		return ('\0');
	return (_source[_pos]);
}

char Lexer::advance(void)
{
	char c = _source[_pos++];

	if (c == '\n')
	{
		_line++;
		_column = 1;
	}
	else
		_column++;
	return (c);
}

bool Lexer::isDelimiter(char c) const
{
	return (std::isspace(static_cast<unsigned char>(c))
		|| c == '{' || c == '}' || c == ';' || c == '#');
}

/* ------------------------------------------------------------------ */
/* Scanning                                                             */
/* ------------------------------------------------------------------ */

void Lexer::skipWhitespaceAndComments(void)
{
	while (!isAtEnd())
	{
		char c = peek();

		if (std::isspace(static_cast<unsigned char>(c)))
		{
			advance();
			continue;
		}
		if (c == '#')
		{
			while (!isAtEnd() && peek() != '\n')
				advance();
			continue;
		}
		break;
	}
}

Token Lexer::readWord(int line, int column)
{
	std::string value;

	while (!isAtEnd() && !isDelimiter(peek()))
		value += advance();
	return (Token(TOKEN_WORD, value, line, column));
}

Token Lexer::nextToken(void)
{
	skipWhitespaceAndComments();

	int line = _line;
	int column = _column;

	if (isAtEnd())
		return (Token(TOKEN_EOF, "", line, column));

	char c = peek();
	if (c == '{')
	{
		advance();
		return (Token(TOKEN_LBRACE, "{", line, column));
	}
	if (c == '}')
	{
		advance();
		return (Token(TOKEN_RBRACE, "}", line, column));
	}
	if (c == ';')
	{
		advance();
		return (Token(TOKEN_SEMICOLON, ";", line, column));
	}
	return (readWord(line, column));
}

std::vector<Token> Lexer::tokenize(void)
{
	std::vector<Token>	tokens;
	Token				token;

	do
	{
		token = nextToken();
		tokens.push_back(token);
	}
	while (token.type != TOKEN_EOF);
	return (tokens);
}

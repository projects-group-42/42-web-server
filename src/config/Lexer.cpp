/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Lexer.cpp                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/12 20:27:44 by dajesus-          #+#    #+#             */
/*   Updated: 2026/08/01 12:54:24 by jucoelho         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

# include "config/Lexer.hpp"
# include <cctype>

/* ------------------------------------------------------------------ */
/* Token                                                              */
/* ------------------------------------------------------------------ */

/**
 * @brief Default constructor for Token. 
 * Initializes an empty EOF token at position 0:0.
 */
Token::Token(void) : type(TOKEN_EOF), value(""), line(0), column(0)
{
}

/**
 * @brief Parameterized constructor for Token.
 * * @param type   The type of the token (e.g., TOKEN_WORD, TOKEN_LBRACE).
 * @param value  The string value or literal of the token.
 * @param line   The line number where the token starts.
 * @param column The column number where the token starts.
 */
Token::Token(t_token_type type, const std::string &value, int line, int column)
	: type(type), value(value), line(line), column(column)
{
}

/* ------------------------------------------------------------------ */
/* Lexer - canonical form                                             */
/* ------------------------------------------------------------------ */

/**
 * @brief Default constructor for Lexer.
 * Initializes an empty lexer with a starting position of line 1, column 1.
 */
Lexer::Lexer(void) : _source(""), _pos(0), _line(1), _column(1)
{
}

/**
 * @brief Parameterized constructor for Lexer.
 * * @param source The raw configuration string to be tokenized.
 */
Lexer::Lexer(const std::string &source)
	: _source(source), _pos(0), _line(1), _column(1)
{
}

/**
 * @brief Copy constructor for Lexer.
 * * @param copy The Lexer instance to be copied.
 */
Lexer::Lexer(const Lexer &copy)
	: _source(copy._source), _pos(copy._pos),
	  _line(copy._line), _column(copy._column)
{
}

/**
 * @brief Assignment operator for Lexer.
 * * @param other The Lexer instance to assign from.
 * @return A reference to the updated Lexer instance.
 */
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

/**
 * @brief Destructor for Lexer.
 */
Lexer::~Lexer(void)
{
}

/* ------------------------------------------------------------------ */
/* Cursor helpers                                                     */
/* ------------------------------------------------------------------ */

/**
 * @brief Checks if the lexer has reached the end of the source string.
 * * @return true if the current position is at or beyond the string size, false otherwise.
 */
bool Lexer::isAtEnd(void) const
{
	return (_pos >= _source.size());
}

/**
 * @brief Looks at the current character without advancing the cursor position.
 * * @return The current character, or '\0' if the end of the source is reached.
 */
char Lexer::peek(void) const
{
	if (isAtEnd())
		return ('\0');
	return (_source[_pos]);
}

/**
 * @brief Consumes the current character and advances the cursor position.
 * It also correctly updates the current line and column trackers.
 * * @return The consumed character.
 */
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
}/**
 * @brief Advances the cursor, skipping all whitespaces and comments.
 * Comments start with '#' and end at the newline character.
 */

/**
 * @brief Checks if a character is considered a token delimiter.
 * Delimiters include whitespaces, curly braces, semicolons, and comment hashes.
 * * @param c The character to evaluate.
 * @return true if the character is a delimiter, false otherwise.
 */
bool Lexer::isDelimiter(char c) const
{
	return (std::isspace(static_cast<unsigned char>(c))
		|| c == '{' || c == '}' || c == ';' || c == '#');
}

/* ------------------------------------------------------------------ */
/* Scanning                                                            */
/* ------------------------------------------------------------------ */

/**
 * @brief Advances the cursor, skipping all whitespaces and comments.
 * Comments start with '#' and end at the newline character.
 */
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

/**
 * @brief Reads a sequence of characters until a delimiter is found.
 * * @param line   The line where the word started.
 * @param column The column where the word started.
 * @return A Token object representing the extracted word (TOKEN_WORD).
 */
Token Lexer::readWord(int line, int column)
{
	std::string value;

	while (!isAtEnd() && !isDelimiter(peek()))
		value += advance();
	return (Token(TOKEN_WORD, value, line, column));
}

/**
 * @brief Identifies and extracts the next token from the source string.
 * Skips spaces and comments, and returns the appropriate Token type 
 * (braces, semicolon, word, or EOF).
 * * @return The next Token parsed from the source.
 */
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

/**
 * @brief Tokenizes the entire source string into a sequence of tokens.
 * Loops over the source extracting tokens until TOKEN_EOF is reached.
 * * @return A std::vector containing all the extracted Tokens.
 */
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

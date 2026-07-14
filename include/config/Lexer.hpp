/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Lexer.hpp                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dajesus- <dajesus-@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/12 17:03:38 by dajesus-          #+#    #+#             */
/*   Updated: 2026/07/13 23:04:05 by dajesus-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef LEXER_HPP
# define LEXER_HPP

# include <string>
# include <vector>

typedef enum e_token_type
{
	TOKEN_WORD,
	TOKEN_LBRACE,
	TOKEN_RBRACE,
	TOKEN_SEMICOLON,
	TOKEN_EOF
}	t_token_type;

/*
 * Token
 *
 * Smallest lexical unit produced by the Lexer. `line`/`column` are
 * 1-based and point at the first character of the token, so a future
 * ConfigParser can report precise syntax errors.
 */
struct Token
{
	t_token_type	type;
	std::string		value;
	int				line;
	int				column;

	Token(void);
	Token(t_token_type type, const std::string &value, int line, int column);
};

/*
 * Lexer
 *
 * Tokenizes an NGINX-like configuration source into a flat stream of
 * Tokens: bare words, '{', '}' and ';'. Whitespace is skipped and '#'
 * starts a line comment. Purely lexical: it does not know about
 * directives, blocks or grammar — that is the future ConfigParser's job.
 */
class Lexer
{
	private:
		std::string	_source;
		size_t		_pos;
		int			_line;
		int			_column;

		bool	isAtEnd(void) const;
		char	peek(void) const;
		char	advance(void);
		bool	isDelimiter(char c) const;
		void	skipWhitespaceAndComments(void);
		Token	readWord(int line, int column);

	public:
		Lexer(void);
		Lexer(const std::string &source);
		Lexer(const Lexer &copy);
		Lexer& operator=(const Lexer &other);
		~Lexer(void);

		Token				nextToken(void);
		std::vector<Token>	tokenize(void);
};

#endif

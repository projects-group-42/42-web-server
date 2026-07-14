/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   config_lexer_test.cpp                              :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dajesus- <dajesus-@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/13 21:16:05 by dajesus-          #+#    #+#             */
/*   Updated: 2026/07/13 23:37:58 by dajesus-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

# include <iostream>
# include <string>
# include <vector>

# include "config/Lexer.hpp"

/* ------------------------------------------------------------------ */
/* Minimal test framework (C++98, zero dependencies)                   */
/* ------------------------------------------------------------------ */

static int  s_pass = 0;
static int  s_fail = 0;
static int  s_test_num = 0;

# define TEST(cond, name) \
	do { \
		s_test_num++; \
		if (cond) { \
			s_pass++; \
			std::cout << "[PASS] " << name << std::endl; \
		} else { \
			s_fail++; \
			std::cerr << "[FAIL] " << name << std::endl; \
		} \
	} while (0)

# define CHECK_EQ(a, b, msg) TEST((a) == (b), msg)

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

static bool tokenMatches(const Token &tok, t_token_type type,
		const std::string &value, int line, int column)
{
	return (tok.type == type && tok.value == value
		&& tok.line == line && tok.column == column);
}

/* ------------------------------------------------------------------ */
/* Lexer tests                                                         */
/* ------------------------------------------------------------------ */

static void	test_lexer_simple_tokens(void)
{
	Lexer				lexer("server {\n    listen 8080;\n}\n");
	std::vector<Token>	tokens = lexer.tokenize();

	CHECK_EQ(tokens.size(), static_cast<size_t>(7),
	         "simple block yields 7 tokens (incl. EOF)");
	TEST(tokenMatches(tokens[0], TOKEN_WORD, "server", 1, 1),
	     "token[0] = WORD \"server\" @1:1");
	TEST(tokenMatches(tokens[1], TOKEN_LBRACE, "{", 1, 8),
	     "token[1] = '{' @1:8");
	TEST(tokenMatches(tokens[2], TOKEN_WORD, "listen", 2, 5),
	     "token[2] = WORD \"listen\" @2:5");
	TEST(tokenMatches(tokens[3], TOKEN_WORD, "8080", 2, 12),
	     "token[3] = WORD \"8080\" @2:12");
	TEST(tokenMatches(tokens[4], TOKEN_SEMICOLON, ";", 2, 16),
	     "token[4] = ';' @2:16");
	TEST(tokenMatches(tokens[5], TOKEN_RBRACE, "}", 3, 1),
	     "token[5] = '}' @3:1");
	TEST(tokenMatches(tokens[6], TOKEN_EOF, "", 4, 1),
	     "token[6] = EOF @4:1");
}

static void	test_lexer_skips_whitespace_and_comments(void)
{
	Lexer				lexer("  # top comment\n\tserver {} # trailing\n");
	std::vector<Token>	tokens = lexer.tokenize();

	CHECK_EQ(tokens.size(), static_cast<size_t>(4),
	         "comments/whitespace produce no tokens");
	TEST(tokenMatches(tokens[0], TOKEN_WORD, "server", 2, 2),
	     "word after comment line keeps correct line/column");
	CHECK_EQ(tokens[1].type, TOKEN_LBRACE, "second token is '{'");
	CHECK_EQ(tokens[2].type, TOKEN_RBRACE, "third token is '}'");
	CHECK_EQ(tokens[3].type, TOKEN_EOF, "trailing comment ignored, ends in EOF");
}

static void	test_lexer_only_comment(void)
{
	Lexer				lexer("# just a comment, no newline at the end");
	std::vector<Token>	tokens = lexer.tokenize();

	CHECK_EQ(tokens.size(), static_cast<size_t>(1),
	         "comment-only source yields only EOF");
	CHECK_EQ(tokens[0].type, TOKEN_EOF, "single token is EOF");
}

static void	test_lexer_empty_source(void)
{
	Lexer				lexer("");
	std::vector<Token>	tokens = lexer.tokenize();

	CHECK_EQ(tokens.size(), static_cast<size_t>(1),
	         "empty source yields only EOF");
	TEST(tokenMatches(tokens[0], TOKEN_EOF, "", 1, 1),
	     "EOF token starts at 1:1 on empty source");
}

static void	test_lexer_words_keep_special_chars(void)
{
	Lexer				lexer("root /var/www; location ~ \\.php$ {}");
	std::vector<Token>	tokens = lexer.tokenize();

	CHECK_EQ(tokens[0].value, "root", "directive name kept as-is");
	CHECK_EQ(tokens[1].value, "/var/www", "path kept as single word");
	CHECK_EQ(tokens[2].type, TOKEN_SEMICOLON, "semicolon closes directive");
	CHECK_EQ(tokens[3].value, "location", "next directive parsed");
	CHECK_EQ(tokens[4].value, "~", "bare tilde is its own word");
	CHECK_EQ(tokens[5].value, "\\.php$", "regex-like value kept as single word");
	CHECK_EQ(tokens[6].type, TOKEN_LBRACE, "block opens after location value");
}

static void	test_lexer_realistic_config(void)
{
	std::string config =
		"server {\n"
		"    listen 8080;\n"
		"    server_name example.com;\n"
		"    root /var/www;\n"
		"\n"
		"    # static files location\n"
		"    location / {\n"
		"        allowed_methods GET POST;\n"
		"        autoindex off;\n"
		"    }\n"
		"}\n";
	Lexer				lexer(config);
	std::vector<Token>	tokens = lexer.tokenize();

	TEST(tokens.back().type == TOKEN_EOF, "realistic config ends in EOF");

	size_t	braceBalance = 0;
	size_t	wordCount = 0;
	for (size_t i = 0; i < tokens.size(); i++)
	{
		if (tokens[i].type == TOKEN_LBRACE)
			braceBalance++;
		else if (tokens[i].type == TOKEN_RBRACE)
			braceBalance--;
		else if (tokens[i].type == TOKEN_WORD)
			wordCount++;
	}
	CHECK_EQ(braceBalance, static_cast<size_t>(0),
	         "braces are balanced across the whole config");
	TEST(wordCount > 0, "realistic config produces word tokens");
}

int	main(void)
{
	test_lexer_simple_tokens();
	test_lexer_skips_whitespace_and_comments();
	test_lexer_only_comment();
	test_lexer_empty_source();
	test_lexer_words_keep_special_chars();
	test_lexer_realistic_config();

	std::cout << std::endl;
	std::cout << s_pass << " passed, " << s_fail << " failed, "
	          << (s_pass + s_fail) << " total" << std::endl;

	return (s_fail == 0 ? 0 : 1);
}

/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   config_parser_test.cpp                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dajesus- <dajesus-@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/19 15:37:32 by dajesus-          #+#    #+#             */
/*   Updated: 2026/07/19 16:10:53 by dajesus-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

# include <iostream>
# include <string>
# include <vector>
# include <stdexcept>

# include "config/Lexer.hpp"
# include "config/ConfigParser.hpp"

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

static ConfigBlock parseSource(const std::string &source)
{
	Lexer				lexer(source);
	std::vector<Token>	tokens = lexer.tokenize();
	ConfigParser		parser(tokens);

	return (parser.parse());
}

static bool throwsRuntimeError(const std::string &source)
{
	try
	{
		parseSource(source);
	}
	catch (const std::runtime_error &e)
	{
		return (true);
	}
	return (false);
}

/* ------------------------------------------------------------------ */
/* ConfigParser tests                                                   */
/* ------------------------------------------------------------------ */

static void	test_parser_empty_source(void)
{
	ConfigBlock	root = parseSource("");

	CHECK_EQ(root.children.size(), static_cast<size_t>(0),
	         "empty source yields no top-level blocks");
	CHECK_EQ(root.directives.size(), static_cast<size_t>(0),
	         "empty source yields no top-level directives");
}

static void	test_parser_basic_server(void)
{
	ConfigBlock	root = parseSource("server {\n    listen 8080;\n}\n");

	CHECK_EQ(root.children.size(), static_cast<size_t>(1),
	         "one top-level 'server' block parsed");

	const ConfigBlock &server = root.children[0];
	CHECK_EQ(server.name, std::string("server"), "block name is 'server'");
	CHECK_EQ(server.args.size(), static_cast<size_t>(0),
	         "'server' takes no arguments");
	CHECK_EQ(server.directives.size(), static_cast<size_t>(1),
	         "'server' has one directive");
	CHECK_EQ(server.directives[0].name, std::string("listen"),
	         "directive name is 'listen'");
	CHECK_EQ(server.directives[0].args.size(), static_cast<size_t>(1),
	         "'listen' has one argument");
	CHECK_EQ(server.directives[0].args[0], std::string("8080"),
	         "'listen' argument is '8080'");
}

static void	test_parser_nested_location(void)
{
	std::string config =
		"server {\n"
		"    listen 8080;\n"
		"    root /var/www;\n"
		"\n"
		"    location / {\n"
		"        allowed_methods GET POST;\n"
		"        autoindex off;\n"
		"    }\n"
		"}\n";
	ConfigBlock	root = parseSource(config);

	CHECK_EQ(root.children.size(), static_cast<size_t>(1), "one server block");

	const ConfigBlock &server = root.children[0];
	CHECK_EQ(server.directives.size(), static_cast<size_t>(2),
	         "server has 'listen' and 'root' directives");
	CHECK_EQ(server.children.size(), static_cast<size_t>(1),
	         "server has one nested 'location' block");

	const ConfigBlock &location = server.children[0];
	CHECK_EQ(location.name, std::string("location"), "nested block is 'location'");
	CHECK_EQ(location.args.size(), static_cast<size_t>(1),
	         "'location' has one argument (the path)");
	CHECK_EQ(location.args[0], std::string("/"), "location path is '/'");
	CHECK_EQ(location.directives.size(), static_cast<size_t>(2),
	         "location has 'allowed_methods' and 'autoindex' directives");
	CHECK_EQ(location.directives[0].args.size(), static_cast<size_t>(2),
	         "'allowed_methods' has two arguments");
}

static void	test_parser_multiple_top_level_servers(void)
{
	std::string config =
		"server {\n"
		"    listen 8080;\n"
		"}\n"
		"server {\n"
		"    listen 8081;\n"
		"}\n";
	ConfigBlock	root = parseSource(config);

	CHECK_EQ(root.children.size(), static_cast<size_t>(2),
	         "two top-level 'server' blocks parsed");
	CHECK_EQ(root.children[0].directives[0].args[0], std::string("8080"),
	         "first server listens on 8080");
	CHECK_EQ(root.children[1].directives[0].args[0], std::string("8081"),
	         "second server listens on 8081");
}

static void	test_parser_comments_are_ignored(void)
{
	std::string config =
		"# top comment\n"
		"server { # inline comment\n"
		"    listen 8080; # another one\n"
		"}\n";
	ConfigBlock	root = parseSource(config);

	CHECK_EQ(root.children.size(), static_cast<size_t>(1),
	         "comments do not disturb block parsing");
	CHECK_EQ(root.children[0].directives.size(), static_cast<size_t>(1),
	         "comments do not disturb directive parsing");
}

static void	test_parser_directive_error_positions(void)
{
	try
	{
		parseSource("server {\n    listen 8080\n}\n");
		TEST(false, "missing ';' throws std::runtime_error");
	}
	catch (const std::runtime_error &e)
	{
		std::string	msg = e.what();
		TEST(msg.find("line 3") != std::string::npos,
		     "missing ';' error reports the correct line");
	}
}

static void	test_parser_missing_semicolon_throws(void)
{
	TEST(throwsRuntimeError("server {\n    listen 8080\n}\n"),
	     "directive without ';' throws instead of crashing");
}

static void	test_parser_unterminated_block_throws(void)
{
	TEST(throwsRuntimeError("server {\n    listen 8080;\n"),
	     "unterminated block throws instead of crashing");
}

static void	test_parser_stray_closing_brace_throws(void)
{
	TEST(throwsRuntimeError("}\n"),
	     "stray '}' at top level throws instead of crashing");
}

static void	test_parser_missing_name_throws(void)
{
	TEST(throwsRuntimeError("{ listen 8080; }\n"),
	     "block without a name throws instead of crashing");
}

int	main(void)
{
	test_parser_empty_source();
	test_parser_basic_server();
	test_parser_nested_location();
	test_parser_multiple_top_level_servers();
	test_parser_comments_are_ignored();
	test_parser_directive_error_positions();
	test_parser_missing_semicolon_throws();
	test_parser_unterminated_block_throws();
	test_parser_stray_closing_brace_throws();
	test_parser_missing_name_throws();

	std::cout << std::endl;
	std::cout << s_pass << " passed, " << s_fail << " failed, "
	          << (s_pass + s_fail) << " total" << std::endl;

	return (s_fail == 0 ? 0 : 1);
}

/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   cgi_handler_test.cpp                               :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: galves-a <galves-a@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/19 00:00:00 by galves-a          #+#    #+#             */
/*   Updated: 2026/07/19 00:00:00 by galves-a         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <iostream>
#include <fstream>
#include <string>
#include <cstdio>

#include "cgi/CgiHandler.hpp"

static int	s_pass = 0;
static int	s_fail = 0;

#define TEST(cond, name) \
	do { \
		if (cond) { s_pass++; std::cout << "[PASS] " << name << std::endl; } \
		else { s_fail++; std::cerr << "[FAIL] " << name << std::endl; } \
	} while (0)

/*
 * Writes content into path so the test can point a CGI run at a real file.
 */
static void	writeScript(const std::string &path, const std::string &content)
{
	std::ofstream	file(path.c_str());

	file << content;
	file.close();
}

/*
 * Runs a script that writes to stdout and checks the parent captured it.
 */
static void	test_stdout_redirect(void)
{
	CgiHandler	handler;
	std::string	output;
	bool		ok;

	writeScript("cgi_echo.sh", "echo hello-cgi\n");
	ok = handler.execute("/bin/sh", "cgi_echo.sh", "", output);
	TEST(ok, "execute returns true on success");
	TEST(output == "hello-cgi\n", "captures script stdout");
	std::remove("cgi_echo.sh");
}

/*
 * Runs a script that echoes its stdin and checks the request body reached it.
 */
static void	test_stdin_redirect(void)
{
	CgiHandler	handler;
	std::string	output;
	bool		ok;

	writeScript("cgi_cat.sh", "cat\n");
	ok = handler.execute("/bin/sh", "cgi_cat.sh", "ping", output);
	TEST(ok, "execute returns true when feeding stdin");
	TEST(output == "ping", "forwards request body to script stdin");
	std::remove("cgi_cat.sh");
}

/*
 * Runs a script that echoes a body larger than the pipe buffer, checking the
 * parent interleaves writing and reading instead of deadlocking on the write.
 */
static void	test_large_body(void)
{
	CgiHandler	handler;
	std::string	body(1024 * 1024, 'x');
	std::string	output;
	bool		ok;

	writeScript("cgi_cat.sh", "cat\n");
	ok = handler.execute("/bin/sh", "cgi_cat.sh", body, output);
	TEST(ok, "execute returns true on a large body");
	TEST(output == body, "streams a body larger than the pipe buffer");
	std::remove("cgi_cat.sh");
}

int	main(void)
{
	test_stdout_redirect();
	test_stdin_redirect();
	test_large_body();
	std::cout << std::endl << s_pass << " passed, " << s_fail
		<< " failed" << std::endl;
	return (s_fail == 0 ? 0 : 1);
}

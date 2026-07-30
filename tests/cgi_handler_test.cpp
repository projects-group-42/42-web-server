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
#include <vector>
#include <cstdio>

#include "cgi/CgiHandler.hpp"
#include "http/HttpRequest.hpp"

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
	CgiHandler					handler;
	std::vector<std::string>	env;
	std::string					output;
	bool						ok;

	writeScript("cgi_echo.sh", "echo hello-cgi\n");
	ok = handler.execute("/bin/sh", "cgi_echo.sh", "", env, output);
	TEST(ok, "execute returns true on success");
	TEST(output == "hello-cgi\n", "captures script stdout");
	std::remove("cgi_echo.sh");
}

/*
 * Runs a script that echoes its stdin and checks the request body reached it.
 */
static void	test_stdin_redirect(void)
{
	CgiHandler					handler;
	std::vector<std::string>	env;
	std::string					output;
	bool						ok;

	writeScript("cgi_cat.sh", "cat\n");
	ok = handler.execute("/bin/sh", "cgi_cat.sh", "ping", env, output);
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
	CgiHandler					handler;
	std::vector<std::string>	env;
	std::string					body(1024 * 1024, 'x');
	std::string					output;
	bool						ok;

	writeScript("cgi_cat.sh", "cat\n");
	ok = handler.execute("/bin/sh", "cgi_cat.sh", body, env, output);
	TEST(ok, "execute returns true on a large body");
	TEST(output == body, "streams a body larger than the pipe buffer");
	std::remove("cgi_cat.sh");
}

/*
 * Returns true when env contains the given KEY=VALUE entry.
 */
static bool	envHas(const std::vector<std::string> &env, const std::string &entry)
{
	for (size_t i = 0; i < env.size(); ++i)
	{
		if (env[i] == entry)
			return (true);
	}
	return (false);
}

/*
 * Checks buildEnv fills the RFC 3875 meta-variables from the request and the
 * resolved script path, including headers forwarded as HTTP_ variables.
 */
static void	test_build_env(void)
{
	CgiHandler					handler;
	HttpRequest					request;
	std::vector<std::string>	env;

	request.setMethod("POST");
	request.setUri("/cgi-bin/form.py");
	request.setQuery("name=42&lang=c");
	request.setVersion("HTTP/1.1");
	request.setHeaders("Content-Type", "application/x-www-form-urlencoded");
	request.setHeaders("Host", "localhost");
	request.setBody("name=42");
	env = handler.buildEnv(request, "cgi-bin/form.py");
	TEST(envHas(env, "GATEWAY_INTERFACE=CGI/1.1"), "buildEnv sets GATEWAY_INTERFACE");
	TEST(envHas(env, "REQUEST_METHOD=POST"), "buildEnv sets REQUEST_METHOD");
	TEST(envHas(env, "QUERY_STRING=name=42&lang=c"), "buildEnv sets QUERY_STRING");
	TEST(envHas(env, "SERVER_PROTOCOL=HTTP/1.1"), "buildEnv sets SERVER_PROTOCOL");
	TEST(envHas(env, "SCRIPT_FILENAME=cgi-bin/form.py"), "buildEnv sets SCRIPT_FILENAME");
	TEST(envHas(env, "SCRIPT_NAME=/cgi-bin/form.py"), "buildEnv sets SCRIPT_NAME");
	TEST(envHas(env, "CONTENT_LENGTH=7"), "buildEnv sets CONTENT_LENGTH from body size");
	TEST(envHas(env, "CONTENT_TYPE=application/x-www-form-urlencoded"), "buildEnv sets CONTENT_TYPE");
	TEST(envHas(env, "HTTP_HOST=localhost"), "buildEnv forwards headers as HTTP_ variables");
}

/*
 * Runs a script that echoes CGI variables and checks the child process received
 * the environment built by buildEnv.
 */
static void	test_env_reaches_script(void)
{
	CgiHandler					handler;
	HttpRequest					request;
	std::vector<std::string>	env;
	std::string					output;
	bool						ok;

	request.setMethod("GET");
	request.setUri("/cgi-bin/env.py");
	request.setQuery("q=hello");
	request.setVersion("HTTP/1.1");
	env = handler.buildEnv(request, "cgi_env.sh");
	writeScript("cgi_env.sh", "echo \"$REQUEST_METHOD:$QUERY_STRING\"\n");
	ok = handler.execute("/bin/sh", "cgi_env.sh", request.getBody(), env, output);
	TEST(ok, "execute returns true with an environment");
	TEST(output == "GET:q=hello\n", "child process receives CGI variables");
	std::remove("cgi_env.sh");
}

int	main(void)
{
	test_stdout_redirect();
	test_stdin_redirect();
	test_large_body();
	test_build_env();
	test_env_reaches_script();
	std::cout << std::endl << s_pass << " passed, " << s_fail
		<< " failed" << std::endl;
	return (s_fail == 0 ? 0 : 1);
}

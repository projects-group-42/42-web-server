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
#include "cgi/CgiProcess.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"
#include <poll.h>

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
 * Runs a script the way the event loop does: a CgiProcess whose two pipes are
 * driven by poll(), one step at a time. The server no longer ships a blocking
 * helper for this, so the tests own the loop and keep reporting the same pair
 * the removed CgiHandler::execute() reported: whether the child exited cleanly
 * and what it wrote.
 */
static bool	runCgi(const std::string &interpreter, const std::string &scriptPath,
			const std::string &body, const std::vector<std::string> &env,
			std::string &output)
{
	CgiProcess	proc(-1, body);

	if (!proc.start(interpreter, scriptPath, env))
		return (false);
	while (!proc.finished())
	{
		struct pollfd	fds[2];
		nfds_t			count = 0;
		int				outIndex = -1;
		int				bodyIndex = -1;

		if (proc.isReading())
		{
			fds[count].fd = proc.outputReadFd();
			fds[count].events = POLLIN;
			fds[count].revents = 0;
			outIndex = static_cast<int>(count);
			++count;
		}
		if (proc.isWriting())
		{
			fds[count].fd = proc.bodyWriteFd();
			fds[count].events = POLLOUT;
			fds[count].revents = 0;
			bodyIndex = static_cast<int>(count);
			++count;
		}
		if (count == 0 || poll(fds, count, 5000) <= 0)
			break ;
		if (proc.isWriting()
			&& (fds[bodyIndex].revents & (POLLOUT | POLLERR | POLLHUP)))
		{
			if (fds[bodyIndex].revents & (POLLERR | POLLHUP))
				proc.stopWriting();
			else
				proc.onWritable();
		}
		if (proc.isReading()
			&& (fds[outIndex].revents & (POLLIN | POLLHUP | POLLERR)))
			proc.onReadable();
	}
	output = proc.output();
	return (proc.reap() == 0);
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
	ok = runCgi("/bin/sh", "cgi_echo.sh", "", env, output);
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
	ok = runCgi("/bin/sh", "cgi_cat.sh", "ping", env, output);
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
	ok = runCgi("/bin/sh", "cgi_cat.sh", body, env, output);
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
	env = handler.buildEnv(request, "cgi-bin/form.py", 8080, "127.0.0.1");
	TEST(envHas(env, "GATEWAY_INTERFACE=CGI/1.1"), "buildEnv sets GATEWAY_INTERFACE");
	TEST(envHas(env, "REQUEST_METHOD=POST"), "buildEnv sets REQUEST_METHOD");
	TEST(envHas(env, "QUERY_STRING=name=42&lang=c"), "buildEnv sets QUERY_STRING");
	TEST(envHas(env, "SERVER_PROTOCOL=HTTP/1.1"), "buildEnv sets SERVER_PROTOCOL");
	TEST(envHas(env, "SCRIPT_FILENAME=cgi-bin/form.py"), "buildEnv sets SCRIPT_FILENAME");
	TEST(envHas(env, "SCRIPT_NAME=/cgi-bin/form.py"), "buildEnv sets SCRIPT_NAME");
	TEST(envHas(env, "CONTENT_LENGTH=7"), "buildEnv sets CONTENT_LENGTH from body size");
	TEST(envHas(env, "CONTENT_TYPE=application/x-www-form-urlencoded"), "buildEnv sets CONTENT_TYPE");
	TEST(envHas(env, "HTTP_HOST=localhost"), "buildEnv forwards headers as HTTP_ variables");
	TEST(envHas(env, "PATH_INFO=cgi-bin/form.py"), "buildEnv sets PATH_INFO to the resolved script");
	TEST(envHas(env, "SERVER_NAME=localhost"), "buildEnv sets SERVER_NAME from the Host header");
	TEST(envHas(env, "SERVER_PORT=8080"), "buildEnv sets SERVER_PORT");
	TEST(envHas(env, "REMOTE_ADDR=127.0.0.1"), "buildEnv sets REMOTE_ADDR");
	TEST(envHas(env, "REQUEST_URI=/cgi-bin/form.py?name=42&lang=c"), "buildEnv sets REQUEST_URI with the query");
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
	env = handler.buildEnv(request, "cgi_env.sh", 8080, "127.0.0.1");
	writeScript("cgi_env.sh", "echo \"$REQUEST_METHOD:$QUERY_STRING\"\n");
	ok = runCgi("/bin/sh", "cgi_env.sh", request.getBody(), env, output);
	TEST(ok, "execute returns true with an environment");
	TEST(output == "GET:q=hello\n", "child process receives CGI variables");
	std::remove("cgi_env.sh");
}

/*
 * Checks parseCgiOutput consumes the Status header into the status code and
 * forwards the other headers and the body of a CRLF-delimited CGI response.
 */
static void	test_parse_status_and_headers(void)
{
	CgiHandler		handler;
	HttpResponse	response;

	TEST(handler.parseCgiOutput(
		"Status: 201 Created\r\nContent-Type: text/plain\r\nX-Foo: bar\r\n\r\nhello body",
		response), "parseCgiOutput accepts a well-formed CGI response");
	TEST(response.getStatusCode() == 201, "parseCgiOutput reads Status code");
	TEST(response.getHeaderValue("Content-Type") == "text/plain", "parseCgiOutput forwards Content-Type");
	TEST(response.getHeaderValue("X-Foo") == "bar", "parseCgiOutput forwards custom headers");
	TEST(response.getBody() == "hello body", "parseCgiOutput extracts the body");
}

/*
 * Checks the status defaults to 200 when the CGI output has no Status header.
 */
static void	test_parse_default_status(void)
{
	CgiHandler		handler;
	HttpResponse	response;

	TEST(handler.parseCgiOutput("Content-Type: text/html\r\n\r\n<h1>hi</h1>", response),
		"parseCgiOutput accepts a response without Status");
	TEST(response.getStatusCode() == 200, "parseCgiOutput defaults to 200 without Status");
	TEST(response.getHeaderValue("Content-Type") == "text/html", "parseCgiOutput keeps Content-Type without Status");
	TEST(response.getBody() == "<h1>hi</h1>", "parseCgiOutput extracts body without Status");
}

/*
 * Checks a redirect Status code is applied and the Location header is kept.
 */
static void	test_parse_redirect(void)
{
	CgiHandler		handler;
	HttpResponse	response;

	TEST(handler.parseCgiOutput("Status: 302 Found\r\nLocation: /next\r\n\r\n", response),
		"parseCgiOutput accepts a redirect response");
	TEST(response.getStatusCode() == 302, "parseCgiOutput reads redirect Status");
	TEST(response.getHeaderValue("Location") == "/next", "parseCgiOutput forwards Location");
	TEST(response.getBody() == "", "parseCgiOutput accepts an empty body with headers");
}

/*
 * Checks a CGI response delimited by a bare LF blank line is parsed too.
 */
static void	test_parse_lf_separator(void)
{
	CgiHandler		handler;
	HttpResponse	response;

	TEST(handler.parseCgiOutput("Content-Type: text/plain\nStatus: 404 Not Found\n\nmissing", response),
		"parseCgiOutput accepts an LF-delimited response");
	TEST(response.getStatusCode() == 404, "parseCgiOutput reads Status with LF separator");
	TEST(response.getHeaderValue("Content-Type") == "text/plain", "parseCgiOutput forwards header with LF separator");
	TEST(response.getBody() == "missing", "parseCgiOutput extracts body with LF separator");
}

/*
 * Checks output without a header separator is rejected instead of being served
 * as a 200 body, so the caller answers 502.
 */
static void	test_parse_missing_separator(void)
{
	CgiHandler		handler;
	HttpResponse	response;

	TEST(!handler.parseCgiOutput("just a plain body\n", response),
		"parseCgiOutput rejects output without a header separator");
	TEST(!handler.parseCgiOutput("Content-Type: text/plain\r\n", response),
		"parseCgiOutput rejects headers that are not terminated by a blank line");
}

/*
 * Checks a script that dies without writing anything is rejected.
 */
static void	test_parse_empty_output(void)
{
	CgiHandler		handler;
	HttpResponse	response;

	TEST(!handler.parseCgiOutput("", response),
		"parseCgiOutput rejects empty output");
	TEST(!handler.parseCgiOutput("\r\n\r\nbody", response),
		"parseCgiOutput rejects an empty header section");
}

/*
 * Checks a header block holding a line that is not a header aborts the parsing
 * instead of being skipped.
 */
static void	test_parse_malformed_headers(void)
{
	CgiHandler		handler;
	HttpResponse	response;

	TEST(!handler.parseCgiOutput("Content-Type: text/plain\r\ngarbage line\r\n\r\nbody", response),
		"parseCgiOutput rejects a header line without a colon");
	TEST(!handler.parseCgiOutput("Traceback (most recent call last):\r\n\r\nbody", response),
		"parseCgiOutput rejects a header name that is not a token");
	TEST(!handler.parseCgiOutput(": nokey\r\n\r\nbody", response),
		"parseCgiOutput rejects an empty header name");
}

/*
 * Checks a Status header the script did not format properly is rejected
 * instead of silently falling back to 200.
 */
static void	test_parse_invalid_status(void)
{
	CgiHandler		handler;
	HttpResponse	response;

	TEST(!handler.parseCgiOutput("Status: not-a-code\r\n\r\nbody", response),
		"parseCgiOutput rejects a non-numeric Status");
	TEST(!handler.parseCgiOutput("Status: 99\r\n\r\nbody", response),
		"parseCgiOutput rejects a Status below 100");
	TEST(!handler.parseCgiOutput("Status: 700 Nope\r\n\r\nbody", response),
		"parseCgiOutput rejects a Status above 599");
}

/*
 * Checks a script is resolved through the prefix of the location serving it,
 * so "/cgi-bin/x.py" served by a location rooted in "cgi-bin" lands on
 * "cgi-bin/x.py" instead of doubling the root, and a URI carrying no prefix
 * resolves to the same path.
 */
static void	test_validate_mount_prefix(void)
{
	CgiHandler		handler;
	HttpResponse	response;
	std::string		prefixed;
	std::string		bare;

	writeScript("cgi-bin/probe_cgi.py", "print()\n");
	handler.setLocationPrefix("/cgi-bin");
	TEST(handler.validate("/cgi-bin/probe_cgi.py", prefixed, response),
		"validate accepts the /cgi-bin prefixed URI");
	TEST(prefixed == "cgi-bin/probe_cgi.py", "validate resolves prefixed URI without doubling the root");
	handler.setLocationPrefix("");
	TEST(handler.validate("/probe_cgi.py", bare, response),
		"validate accepts the bare URI");
	TEST(bare == "cgi-bin/probe_cgi.py", "validate resolves the bare URI to the same path");
	std::remove("cgi-bin/probe_cgi.py");
}

/*
 * Checks parseCgiOutput keeps every occurrence of a repeated header instead of
 * collapsing them, so multiple Set-Cookie lines survive.
 */
static void	test_parse_duplicate_headers(void)
{
	CgiHandler		handler;
	HttpResponse	response;
	int				cookies = 0;

	TEST(handler.parseCgiOutput(
		"Content-Type: text/html\r\nSet-Cookie: a=1\r\nSet-Cookie: b=2\r\n\r\nbody",
		response), "parseCgiOutput accepts a response with repeated headers");
	const std::vector<std::pair<std::string, std::string> >	&headers = response.getHeaders();
	for (size_t i = 0; i < headers.size(); ++i)
	{
		if (headers[i].first == "set-cookie")
			++cookies;
	}
	TEST(cookies == 2, "parseCgiOutput preserves duplicate Set-Cookie headers");
}

int	main(void)
{
	test_stdout_redirect();
	test_stdin_redirect();
	test_large_body();
	test_build_env();
	test_env_reaches_script();
	test_parse_status_and_headers();
	test_parse_default_status();
	test_parse_redirect();
	test_parse_lf_separator();
	test_parse_missing_separator();
	test_parse_empty_output();
	test_parse_malformed_headers();
	test_parse_invalid_status();
	test_validate_mount_prefix();
	test_parse_duplicate_headers();
	std::cout << std::endl << s_pass << " passed, " << s_fail
		<< " failed" << std::endl;
	return (s_fail == 0 ? 0 : 1);
}

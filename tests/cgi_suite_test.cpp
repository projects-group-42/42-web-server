/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   cgi_suite_test.cpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dajesus- <dajesus-@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/08/03 12:33:35 by dajesus-          #+#    #+#             */
/*   Updated: 2026/08/03 13:42:22 by dajesus-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <iostream>
#include <string>
#include <vector>
#include <poll.h>

#include "cgi/CgiHandler.hpp"
#include "cgi/CgiProcess.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"

static int	s_pass = 0;
static int	s_fail = 0;

#define TEST(cond, name) \
	do { \
		if (cond) { s_pass++; std::cout << "[PASS] " << name << std::endl; } \
		else { s_fail++; std::cerr << "[FAIL] " << name << std::endl; } \
	} while (0)

static const std::string	PYTHON3 = "/usr/bin/python3";

/*
 * Drives the process to completion the way the event loop does: polls the
 * active pipe fds and calls a single incremental step per ready fd, never
 * blocking on either direction.
 */
static void	driveToCompletion(CgiProcess &proc)
{
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
		if (count == 0 || poll(fds, count, 2000) <= 0)
			break;
		if (proc.isWriting() && (fds[bodyIndex].revents & (POLLOUT | POLLERR | POLLHUP)))
		{
			if (fds[bodyIndex].revents & (POLLERR | POLLHUP))
				proc.stopWriting();
			else
				proc.onWritable();
		}
		if (proc.isReading() && (fds[outIndex].revents & (POLLIN | POLLHUP | POLLERR)))
			proc.onReadable();
	}
}

/* ------------------------------------------------------------------------- */
/*  CgiHandler::execute() — Python CGI scripts                                */
/* ------------------------------------------------------------------------- */

/*
 * Runs the echo.py script through /usr/bin/python3 and checks the output is
 * captured and can be parsed into a valid CGI response.
 */
static void	test_python_echo(void)
{
	CgiHandler					handler;
	std::vector<std::string>	env;
	std::string					output;
	HttpResponse				response;
	bool						ok;

	env.push_back("GATEWAY_INTERFACE=CGI/1.1");
	env.push_back("REQUEST_METHOD=GET");
	env.push_back("QUERY_STRING=");
	env.push_back("SERVER_PROTOCOL=HTTP/1.1");
	env.push_back("SCRIPT_NAME=/cgi-bin/echo.py");
	env.push_back("SCRIPT_FILENAME=cgi-bin/echo.py");
	env.push_back("CONTENT_LENGTH=4");
	env.push_back("REDIRECT_STATUS=200");
	ok = handler.execute(PYTHON3, "cgi-bin/echo.py", "data", env, output);
	TEST(ok, "python echo.py: execute returns true");
	TEST(!output.empty(), "python echo.py: captured output");
	TEST(handler.parseCgiOutput(output, response), "python echo.py: parses CGI output");
	TEST(response.getStatusCode() == 200, "python echo.py: status defaults to 200");
	TEST(response.getHeaderValue("Content-Type") == "text/plain", "python echo.py: has Content-Type");
	TEST(response.getBody() == "echo:data", "python echo.py: body contains echoed stdin");
}

/*
 * Runs the headers.py script, which sets Status: 201 and a custom X-Cgi-Test
 * header, and checks parseCgiOutput reads the custom status and headers.
 */
static void	test_python_headers(void)
{
	CgiHandler					handler;
	std::vector<std::string>	env;
	std::string					output;
	HttpResponse				response;
	bool						ok;

	env.push_back("GATEWAY_INTERFACE=CGI/1.1");
	env.push_back("REQUEST_METHOD=GET");
	env.push_back("QUERY_STRING=");
	env.push_back("SERVER_PROTOCOL=HTTP/1.1");
	env.push_back("SCRIPT_NAME=/cgi-bin/headers.py");
	env.push_back("SCRIPT_FILENAME=cgi-bin/headers.py");
	env.push_back("CONTENT_LENGTH=0");
	env.push_back("REDIRECT_STATUS=200");
	ok = handler.execute(PYTHON3, "cgi-bin/headers.py", "", env, output);
	TEST(ok, "python headers.py: execute returns true");
	TEST(handler.parseCgiOutput(output, response), "python headers.py: parses CGI output");
	TEST(response.getStatusCode() == 201, "python headers.py: reads 201 Status");
	TEST(response.getHeaderValue("Content-Type") == "text/plain", "python headers.py: Content-Type correct");
	TEST(response.getHeaderValue("X-Cgi-Test") == "ok", "python headers.py: custom header survives");
	TEST(response.getBody() == "cgi-body:", "python headers.py: body with empty stdin");
}

/*
 * Runs the query_echo.py script, which reads QUERY_STRING and REQUEST_METHOD
 * from the environment and echoes them back. Checks the Python process
 * actually receives the environment variables.
 */
static void	test_python_query_string(void)
{
	CgiHandler					handler;
	std::vector<std::string>	env;
	std::string					output;
	HttpResponse				response;
	bool						ok;

	env.push_back("GATEWAY_INTERFACE=CGI/1.1");
	env.push_back("REQUEST_METHOD=GET");
	env.push_back("QUERY_STRING=a=1&b=2");
	env.push_back("SERVER_PROTOCOL=HTTP/1.1");
	env.push_back("SCRIPT_NAME=/cgi-bin/query_echo.py");
	env.push_back("SCRIPT_FILENAME=cgi-bin/query_echo.py");
	env.push_back("CONTENT_LENGTH=0");
	env.push_back("REDIRECT_STATUS=200");
	ok = handler.execute(PYTHON3, "cgi-bin/query_echo.py", "", env, output);
	TEST(ok, "python query_echo: execute returns true");
	TEST(handler.parseCgiOutput(output, response), "python query_echo: parses CGI output");
	TEST(response.getStatusCode() == 200, "python query_echo: status 200");
	TEST(response.getBody() == "GET:a=1&b=2", "python query_echo: receives method and query");
}

/*
 * Runs the post_echo.py script with a POST body and checks the body is
 * forwarded to Python and echoed back with Content-Length and X-Body-Size
 * headers matching the input.
 */
static void	test_python_post_body(void)
{
	CgiHandler					handler;
	std::vector<std::string>	env;
	std::string					output;
	HttpResponse				response;
	std::string					body = "name=John&age=30";
	bool						ok;

	env.push_back("GATEWAY_INTERFACE=CGI/1.1");
	env.push_back("REQUEST_METHOD=POST");
	env.push_back("QUERY_STRING=");
	env.push_back("SERVER_PROTOCOL=HTTP/1.1");
	env.push_back("SCRIPT_NAME=/cgi-bin/post_echo.py");
	env.push_back("SCRIPT_FILENAME=cgi-bin/post_echo.py");
	env.push_back("CONTENT_LENGTH=16");
	env.push_back("CONTENT_TYPE=application/x-www-form-urlencoded");
	env.push_back("REDIRECT_STATUS=200");
	ok = handler.execute(PYTHON3, "cgi-bin/post_echo.py", body, env, output);
	TEST(ok, "python post_echo: execute returns true");
	TEST(handler.parseCgiOutput(output, response), "python post_echo: parses CGI output");
	TEST(response.getStatusCode() == 200, "python post_echo: status 200");
	TEST(response.getHeaderValue("X-Body-Size") == "16", "python post_echo: X-Body-Size matches sent length");
	TEST(response.getBody() == body, "python post_echo: body echoes POST data");
}

/*
 * Runs the error_exit.py script, which exits with status 1. execute() must
 * return false because the child did not exit cleanly with 0.
 */
static void	test_python_error_exit(void)
{
	CgiHandler					handler;
	std::vector<std::string>	env;
	std::string					output;
	bool						ok;

	env.push_back("GATEWAY_INTERFACE=CGI/1.1");
	env.push_back("REQUEST_METHOD=GET");
	env.push_back("QUERY_STRING=");
	env.push_back("SERVER_PROTOCOL=HTTP/1.1");
	env.push_back("SCRIPT_NAME=/cgi-bin/error_exit.py");
	env.push_back("SCRIPT_FILENAME=cgi-bin/error_exit.py");
	env.push_back("CONTENT_LENGTH=0");
	env.push_back("REDIRECT_STATUS=200");
	ok = handler.execute(PYTHON3, "cgi-bin/error_exit.py", "", env, output);
	TEST(!ok, "python error_exit: execute returns false on non-zero exit");
}

/*
 * Runs the stderr_mixed.py script, which writes log messages to stderr
 * around the stdout CGI response. Checks that stdout is clean (only the
 * CGI response, no stderr leaked into the pipe).
 */
static void	test_python_stderr_mixed(void)
{
	CgiHandler					handler;
	std::vector<std::string>	env;
	std::string					output;
	HttpResponse				response;
	bool						ok;

	env.push_back("GATEWAY_INTERFACE=CGI/1.1");
	env.push_back("REQUEST_METHOD=GET");
	env.push_back("QUERY_STRING=");
	env.push_back("SERVER_PROTOCOL=HTTP/1.1");
	env.push_back("SCRIPT_NAME=/cgi-bin/stderr_mixed.py");
	env.push_back("SCRIPT_FILENAME=cgi-bin/stderr_mixed.py");
	env.push_back("CONTENT_LENGTH=0");
	env.push_back("REDIRECT_STATUS=200");
	ok = handler.execute(PYTHON3, "cgi-bin/stderr_mixed.py", "", env, output);
	TEST(ok, "python stderr_mixed: execute returns true");
	TEST(handler.parseCgiOutput(output, response), "python stderr_mixed: parses CGI output");
	TEST(response.getStatusCode() == 200, "python stderr_mixed: status 200");
	TEST(response.getHeaderValue("X-Debug") == "on", "python stderr_mixed: custom header present");
	TEST(response.getBody() == "result from stderr_mixed.py", "python stderr_mixed: body clean (no stderr leaked)");
}

/*
 * Runs the large_output.py script, which produces a 65536-byte body. Checks
 * the large output is fully captured by the incremental pump and the body
 * matches exactly.
 */
static void	test_python_large_output(void)
{
	CgiHandler					handler;
	std::vector<std::string>	env;
	std::string					output;
	HttpResponse				response;
	bool						ok;

	env.push_back("GATEWAY_INTERFACE=CGI/1.1");
	env.push_back("REQUEST_METHOD=GET");
	env.push_back("QUERY_STRING=");
	env.push_back("SERVER_PROTOCOL=HTTP/1.1");
	env.push_back("SCRIPT_NAME=/cgi-bin/large_output.py");
	env.push_back("SCRIPT_FILENAME=cgi-bin/large_output.py");
	env.push_back("CONTENT_LENGTH=0");
	env.push_back("REDIRECT_STATUS=200");
	ok = handler.execute(PYTHON3, "cgi-bin/large_output.py", "", env, output);
	TEST(ok, "python large_output: execute returns true");
	TEST(handler.parseCgiOutput(output, response), "python large_output: parses CGI output");
	TEST(response.getStatusCode() == 200, "python large_output: status 200");
	TEST(response.getBody().size() == 65536, "python large_output: body is 65536 bytes");
	TEST(!response.getBody().empty() && response.getBody()[0] == 'x', "python large_output: body content matches");
}

/*
 * Runs the no_output.py script, which exits 0 without writing anything.
 * execute() returns true (exit 0), but parseCgiOutput rejects the empty
 * output, which is the server's signal to answer 502.
 */
static void	test_python_no_output(void)
{
	CgiHandler					handler;
	std::vector<std::string>	env;
	std::string					output;
	HttpResponse				response;
	bool						ok;

	env.push_back("GATEWAY_INTERFACE=CGI/1.1");
	env.push_back("REQUEST_METHOD=GET");
	env.push_back("QUERY_STRING=");
	env.push_back("SERVER_PROTOCOL=HTTP/1.1");
	env.push_back("SCRIPT_NAME=/cgi-bin/no_output.py");
	env.push_back("SCRIPT_FILENAME=cgi-bin/no_output.py");
	env.push_back("CONTENT_LENGTH=0");
	env.push_back("REDIRECT_STATUS=200");
	ok = handler.execute(PYTHON3, "cgi-bin/no_output.py", "", env, output);
	TEST(ok, "python no_output: execute returns true (exit 0)");
	TEST(output.empty(), "python no_output: output is empty");
	TEST(!handler.parseCgiOutput(output, response), "python no_output: parseCgiOutput rejects empty output");
}

/*
 * Runs the status_404.py script, which sets Status: 404 Not Found. Checks
 * parseCgiOutput reads the 404 status code correctly.
 */
static void	test_python_status_404(void)
{
	CgiHandler					handler;
	std::vector<std::string>	env;
	std::string					output;
	HttpResponse				response;
	bool						ok;

	env.push_back("GATEWAY_INTERFACE=CGI/1.1");
	env.push_back("REQUEST_METHOD=GET");
	env.push_back("QUERY_STRING=");
	env.push_back("SERVER_PROTOCOL=HTTP/1.1");
	env.push_back("SCRIPT_NAME=/cgi-bin/status_404.py");
	env.push_back("SCRIPT_FILENAME=cgi-bin/status_404.py");
	env.push_back("CONTENT_LENGTH=0");
	env.push_back("REDIRECT_STATUS=200");
	ok = handler.execute(PYTHON3, "cgi-bin/status_404.py", "", env, output);
	TEST(ok, "python status_404: execute returns true");
	TEST(handler.parseCgiOutput(output, response), "python status_404: parses CGI output");
	TEST(response.getStatusCode() == 404, "python status_404: reads 404 status");
	TEST(response.getHeaderValue("Content-Type") == "text/html", "python status_404: Content-Type correct");
	TEST(response.getBody() == "<h1>CGI 404</h1>", "python status_404: body present");
}

/*
 * Runs the slow.py script, which sleeps 2 seconds before writing. Checks
 * that the parent waits long enough (poll timeout is generous) and still
 * captures the output correctly.
 */
static void	test_python_slow(void)
{
	CgiHandler					handler;
	std::vector<std::string>	env;
	std::string					output;
	HttpResponse				response;
	bool						ok;

	env.push_back("GATEWAY_INTERFACE=CGI/1.1");
	env.push_back("REQUEST_METHOD=GET");
	env.push_back("QUERY_STRING=");
	env.push_back("SERVER_PROTOCOL=HTTP/1.1");
	env.push_back("SCRIPT_NAME=/cgi-bin/slow.py");
	env.push_back("SCRIPT_FILENAME=cgi-bin/slow.py");
	env.push_back("CONTENT_LENGTH=0");
	env.push_back("REDIRECT_STATUS=200");
	ok = handler.execute(PYTHON3, "cgi-bin/slow.py", "", env, output);
	TEST(ok, "python slow: execute returns true after sleep");
	TEST(!output.empty(), "python slow: captured output after delay");
	TEST(handler.parseCgiOutput(output, response), "python slow: parses CGI output");
	TEST(response.getBody() == "slow cgi done", "python slow: body present");
}

/* ------------------------------------------------------------------------- */
/*  CgiProcess — Python CGI scripts                                           */
/* ------------------------------------------------------------------------- */

/*
 * Runs echo.py through CgiProcess with incremental poll-driven I/O. Checks
 * that output is accumulated, the process finishes, and reap returns 0.
 */
static void	test_proc_python_echo(void)
{
	std::vector<std::string>	env;
	CgiProcess					proc(-1, "test-body");

	env.push_back("GATEWAY_INTERFACE=CGI/1.1");
	env.push_back("REQUEST_METHOD=GET");
	env.push_back("QUERY_STRING=");
	env.push_back("SERVER_PROTOCOL=HTTP/1.1");
	env.push_back("SCRIPT_NAME=/cgi-bin/echo.py");
	env.push_back("SCRIPT_FILENAME=cgi-bin/echo.py");
	env.push_back("CONTENT_LENGTH=9");
	env.push_back("REDIRECT_STATUS=200");
	TEST(proc.start(PYTHON3, "cgi-bin/echo.py", env), "proc python echo: start returns true");
	driveToCompletion(proc);
	TEST(proc.finished(), "proc python echo: process finished");
	TEST(proc.output() == "Content-Type: text/plain\r\n\r\necho:test-body", "proc python echo: captured echo output");
	TEST(proc.reap() == 0, "proc python echo: reap returns 0");
}

/*
 * Runs headers.py through CgiProcess, checking the 201 status and custom
 * headers appear in the accumulated output.
 */
static void	test_proc_python_headers(void)
{
	std::vector<std::string>	env;
	CgiProcess					proc(-1, "");

	env.push_back("GATEWAY_INTERFACE=CGI/1.1");
	env.push_back("REQUEST_METHOD=GET");
	env.push_back("QUERY_STRING=");
	env.push_back("SERVER_PROTOCOL=HTTP/1.1");
	env.push_back("SCRIPT_NAME=/cgi-bin/headers.py");
	env.push_back("SCRIPT_FILENAME=cgi-bin/headers.py");
	env.push_back("CONTENT_LENGTH=0");
	env.push_back("REDIRECT_STATUS=200");
	TEST(proc.start(PYTHON3, "cgi-bin/headers.py", env), "proc python headers: start returns true");
	driveToCompletion(proc);
	TEST(proc.finished(), "proc python headers: process finished");
	TEST(proc.reap() == 0, "proc python headers: reap returns 0");
	CgiHandler		handler;
	HttpResponse	response;
	TEST(handler.parseCgiOutput(proc.output(), response), "proc python headers: parseCgiOutput succeeds");
	TEST(response.getStatusCode() == 201, "proc python headers: status 201");
	TEST(response.getHeaderValue("X-Cgi-Test") == "ok", "proc python headers: X-Cgi-Test header");
}

/*
 * Runs echo.py through CgiProcess with a 256KB body, checking the writing
 * direction is active and the whole body reaches the script.
 */
static void	test_proc_python_large_body(void)
{
	std::vector<std::string>	env;
	std::string					body(256 * 1024, 'A');
	CgiProcess					proc(-1, body);

	env.push_back("GATEWAY_INTERFACE=CGI/1.1");
	env.push_back("REQUEST_METHOD=POST");
	env.push_back("QUERY_STRING=");
	env.push_back("SERVER_PROTOCOL=HTTP/1.1");
	env.push_back("SCRIPT_NAME=/cgi-bin/echo.py");
	env.push_back("SCRIPT_FILENAME=cgi-bin/echo.py");
	env.push_back("CONTENT_LENGTH=262144");
	env.push_back("REDIRECT_STATUS=200");
	TEST(proc.start(PYTHON3, "cgi-bin/echo.py", env), "proc python large: start returns true");
	TEST(proc.isWriting(), "proc python large: writing direction active");
	driveToCompletion(proc);
	TEST(proc.finished(), "proc python large: process finished");
	TEST(proc.reap() == 0, "proc python large: reap returns 0");
	CgiHandler		handler;
	HttpResponse	response;
	TEST(handler.parseCgiOutput(proc.output(), response), "proc python large: parseCgiOutput succeeds");
	TEST(response.getBody() == "echo:" + body, "proc python large: 256KB body round-trips");
}

/*
 * Runs error_exit.py through CgiProcess, checking reap returns non-zero
 * (exit status 1) as the server expects for error-producing scripts.
 */
static void	test_proc_python_error_exit(void)
{
	std::vector<std::string>	env;
	CgiProcess					proc(-1, "");

	env.push_back("GATEWAY_INTERFACE=CGI/1.1");
	env.push_back("REQUEST_METHOD=GET");
	env.push_back("QUERY_STRING=");
	env.push_back("SERVER_PROTOCOL=HTTP/1.1");
	env.push_back("SCRIPT_NAME=/cgi-bin/error_exit.py");
	env.push_back("SCRIPT_FILENAME=cgi-bin/error_exit.py");
	env.push_back("CONTENT_LENGTH=0");
	env.push_back("REDIRECT_STATUS=200");
	TEST(proc.start(PYTHON3, "cgi-bin/error_exit.py", env), "proc python error: start returns true");
	driveToCompletion(proc);
	TEST(proc.finished(), "proc python error: process finished");
	TEST(proc.reap() == 1, "proc python error: reap returns exit status 1");
}

/* ------------------------------------------------------------------------- */
/*  Edge cases with parseCgiOutput                                            */
/* ------------------------------------------------------------------------- */

/*
 * Simulates a Python script that crashed and dumped a traceback to stdout
 * instead of a valid CGI response. parseCgiOutput must reject it because
 * the header block contains lines that are not valid HTTP headers.
 */
static void	test_parse_python_traceback(void)
{
	CgiHandler		handler;
	HttpResponse	response;

	TEST(!handler.parseCgiOutput(
		"Traceback (most recent call last):\r\n"
		"  File \"script.py\", line 5, in <module>\r\n"
		"    raise Exception(\"boom\")\r\n"
		"Exception: boom\r\n"
		"\r\n",
		response),
		"parseCgiOutput rejects Python traceback as malformed headers");
}

/*
 * Simulates a Python script that printed debug noise before a valid CGI
 * header block. parseCgiOutput must reject the output because the first
 * lines are not valid headers.
 */
static void	test_parse_debug_before_headers(void)
{
	CgiHandler		handler;
	HttpResponse	response;

	TEST(!handler.parseCgiOutput(
		"Starting script...\r\n"
		"Content-Type: text/plain\r\n"
		"\r\n"
		"body",
		response),
		"parseCgiOutput rejects debug noise before headers");
}

/*
 * Checks that a Python CGI using LF-only line endings (common on Unix) is
 * still parsed correctly for both headers and body.
 */
static void	test_parse_python_lf_style(void)
{
	CgiHandler		handler;
	HttpResponse	response;

	TEST(handler.parseCgiOutput(
		"Status: 301 Moved Permanently\n"
		"Location: /new-home\n"
		"\n"
		"redirecting...",
		response),
		"parseCgiOutput accepts LF-only Python-style output");
	TEST(response.getStatusCode() == 301, "LF Python-style: reads 301 status");
	TEST(response.getHeaderValue("Location") == "/new-home", "LF Python-style: Location header");
	TEST(response.getBody() == "redirecting...", "LF Python-style: body extracted");
}

int	main(void)
{
	/* CgiHandler::execute() with Python scripts */
	test_python_echo();
	test_python_headers();
	test_python_query_string();
	test_python_post_body();
	test_python_error_exit();
	test_python_stderr_mixed();
	test_python_large_output();
	test_python_no_output();
	test_python_status_404();
	test_python_slow();

	/* CgiProcess with Python scripts */
	test_proc_python_echo();
	test_proc_python_headers();
	test_proc_python_large_body();
	test_proc_python_error_exit();

	/* parseCgiOutput edge cases */
	test_parse_python_traceback();
	test_parse_debug_before_headers();
	test_parse_python_lf_style();

	std::cout << std::endl << s_pass << " passed, " << s_fail
		<< " failed" << std::endl;
	return (s_fail == 0 ? 0 : 1);
}

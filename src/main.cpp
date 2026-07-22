/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/31 17:46:15 by jucoelho          #+#    #+#             */
/*   Updated: 2026/06/29 18:39:15 by jucoelho         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <fcntl.h>
#include <signal.h>
#include "webserver.hpp"
#include "http/RequestParser.hpp"


static void test_parser(const std::string& label, const std::string& raw)
{
	Logger::info("--- TEST: " + label + " ---");
	RequestParser parser;
	parser.feed(raw.c_str(), raw.size());
	if (parser.get_psr_state() == COMPLETE)
	{
		const HttpRequest& req = parser.getRequest();
		Logger::info("  State:   COMPLETE");
		Logger::info("  Method:  " + req.getMethod());
		Logger::info("  URI:     " + req.getUri());
		Logger::info("  Query:   " + req.getQuery());
		Logger::info("  Version: " + req.getVersion());
		Logger::info("  Body:    " + req.getBody());
		Logger::info("  Host:    " + req.getHeaderValue("Host"));
	}
	else if (parser.get_psr_state() == ERROR)
		Logger::error("  State:   ERROR");
	else
		Logger::warning("  State:   INCOMPLETE");
}
 
int main(void)
{
	signal(SIGPIPE, SIG_IGN);
	// --- Parser tests ---
	test_parser("GET sem body",
		"GET /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n");
 
	test_parser("GET com query",
		"GET /search?q=foo HTTP/1.1\r\nHost: localhost\r\n\r\n");
 
	test_parser("POST com body",
		"POST /upload HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\n\r\nhello");
 
	test_parser("GET com CRLF iniciais",
		"\r\n\r\nGET /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n");
 
	test_parser("Request incompleta (sem fim de headers)",
		"GET /index.html HTTP/1.1\r\nHost: localhost\r\n");
 
	test_parser("Request invalida (sem metodo)",
		"\r\n\r\n");
	
	test_parser("Query com %20",
		"GET /search?q=Joao%20Silva HTTP/1.1\r\nHost: localhost\r\n\r\n");

	test_parser("URI com %2F",
		"GET /dir%2Ffile HTTP/1.1\r\nHost: localhost\r\n\r\n");

	test_parser("URI com %00",
		"GET /test%00abc HTTP/1.1\r\nHost: localhost\r\n\r\n");

	test_parser("Percent incompleto",
		"GET /test% HTTP/1.1\r\nHost: localhost\r\n\r\n");

	test_parser("Hex incompleto",
		"GET /test%2 HTTP/1.1\r\nHost: localhost\r\n\r\n");

	test_parser("Hex invalido",
		"GET /test%GG HTTP/1.1\r\nHost: localhost\r\n\r\n");
	const std::string	host = "0.0.0.0";
	const int			port = 8080;
	const int			backlog = 128;

	try
	{
		Socket sckt;

		sckt.create();
		sckt.bind(host, port);
		sckt.listen(backlog);
		int flags = fcntl(sckt.getFd(), F_GETFL, 0);
		if (flags != -1 && (flags & O_NONBLOCK))
			Logger::info("Socket is non-blocking.");
		else
			Logger::warning("Socket is blocking.");
		Logger::info("Listening on 0.0.0.0:8080 — connect with: nc localhost 8080");
		EventLoop loop(&sckt);
		loop.run();
	}
	catch (const std::exception &e)
	{
		Logger::error(e.what());
		return 1;
	}
	return 0;
}

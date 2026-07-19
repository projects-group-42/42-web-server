/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/31 17:46:15 by jucoelho          #+#    #+#             */
/*   Updated: 2026/07/19 15:15:29 by jucoelho         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <fcntl.h>
#include "webserver.hpp"
#include "http/RequestParser.hpp"
#include "utils/Logger.hpp"
#include <iostream>
#include <string>
#include <cstring>
#include "http/RequestParser.hpp"
#include "http/HttpRequest.hpp"
#include "utils/Logger.hpp"
#include "network/Socket.hpp"
#include "server/EventLoop.hpp"
#include <unistd.h>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <cstring>
#include <sstream>
#include "http/RequestParser.hpp"
#include "http/HttpRequest.hpp"
#include "utils/Logger.hpp"
#include "network/Socket.hpp"
#include "server/EventLoop.hpp"
#include <unistd.h>
#include <fcntl.h>

// Helper para converter int para string (C++98 compatible)
static std::string intToString(int value)
{
	std::stringstream ss;
	ss << value;
	return ss.str();
}

void testChunkedParser()
{
		Logger::info("\n=== Teste DEBUG: O que está acontecendo em prs_chunked_data ===");
	{
		RequestParser parser;
		std::string request = "POST / HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nHello\r\n0\r\n\r\n";
		
		parser.feed(request.c_str(), request.length());
		
		Logger::info("State: " + intToString(parser.get_psr_state()) + " (esperado 6=COMPLETE)");
		Logger::info("Body received: '" + parser.getRequest().getBody() + "'");
		Logger::info("Body expected: 'Hello'");
		Logger::info("Body length: " + intToString(parser.getRequest().getBody().length()));
	}
	Logger::info("=== Teste 1: Chunked simples (inteiro de uma vez) ===");
	{
		RequestParser parser1;
		std::string request1 = "POST / HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nHello\r\n6\r\n World\r\n0\r\n\r\n";
		
		parser1.feed(request1.c_str(), request1.length());
		Logger::info("Buffer size: " + intToString(parser1.getRequest().getBody().length()));
		Logger::info("State: " + intToString(parser1.get_psr_state()));
		if (parser1.get_psr_state() != COMPLETE)
		{
			Logger::error("Expected COMPLETE state (6)");
		}
		else
		{
			const HttpRequest& req = parser1.getRequest();
			Logger::info("Body: '" + req.getBody() + "'");
			Logger::info("Expected: 'Hello World'");
		}
	}

	Logger::info("\n=== Teste 2: Chunked em pedaços (3 recv) ===");
	{
		RequestParser parser2;
		
		std::string chunk1 = "POST / HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nHel";
		std::string chunk2 = "lo\r\n6\r\n Wor";
		std::string chunk3 = "ld\r\n0\r\n\r\n";
		
		Logger::info("Feeding chunk 1 (" + intToString(chunk1.length()) + " bytes)...");
		parser2.feed(chunk1.c_str(), chunk1.length());
		Logger::info("State after chunk 1: " + intToString(parser2.get_psr_state()));
		
		Logger::info("Feeding chunk 2 (" + intToString(chunk2.length()) + " bytes)...");
		parser2.feed(chunk2.c_str(), chunk2.length());
		Logger::info("State after chunk 2: " + intToString(parser2.get_psr_state()));
		
		Logger::info("Feeding chunk 3 (" + intToString(chunk3.length()) + " bytes)...");
		parser2.feed(chunk3.c_str(), chunk3.length());
		Logger::info("State after chunk 3: " + intToString(parser2.get_psr_state()));
		
		if (parser2.get_psr_state() == COMPLETE)
		{
			const HttpRequest& req = parser2.getRequest();
			Logger::info("Body: '" + req.getBody() + "'");
			Logger::info("Expected: 'Hello World'");
		}
		else
		{
			Logger::error("Expected COMPLETE state (6) but got: " + intToString(parser2.get_psr_state()));
		}
	}

	Logger::info("\n=== Teste 3: Chunked + Content-Length (deve erro) ===");
	{
		RequestParser parser3;
		std::string badRequest = "POST / HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\nContent-Length: 11\r\n\r\n5\r\nHello\r\n6\r\n World\r\n0\r\n\r\n";
		
		parser3.feed(badRequest.c_str(), badRequest.length());
		
		if (parser3.get_psr_state() == ERROR)
		{
			Logger::info("✓ Correctly detected error. Error code: " + intToString(parser3.get_error_code()));
		}
		else
		{
			Logger::warning("✗ Expected ERROR state (7) but got: " + intToString(parser3.get_psr_state()));
		}
	}

	Logger::info("\n=== Teste 4: Chunked malformado (dados != size) ===");
	{
		RequestParser parser4;
		std::string malformed = "POST / HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nHelloWorld\r\n0\r\n\r\n";
		
		parser4.feed(malformed.c_str(), malformed.length());
		
		if (parser4.get_psr_state() == ERROR)
		{
			Logger::info("✓ Correctly detected malformed chunk. Error code: " + intToString(parser4.get_error_code()));
		}
		else
		{
			Logger::warning("✗ Expected ERROR state (7) but got: " + intToString(parser4.get_psr_state()));
		}
	}

	Logger::info("\n=== Teste 5: Chunked vazio ===");
	{
		RequestParser parser5;
		std::string empty = "POST / HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\n";
		
		parser5.feed(empty.c_str(), empty.length());
		
		Logger::info("State: " + intToString(parser5.get_psr_state()));
		if (parser5.get_psr_state() == COMPLETE)
		{
			const HttpRequest& req = parser5.getRequest();
			Logger::info("✓ Body: '" + req.getBody() + "' (empty as expected)");
		}
		else
		{
			Logger::error("✗ Expected COMPLETE but got: " + intToString(parser5.get_psr_state()));
		}
	}

	Logger::info("\n=== Teste 6: Múltiplos chunks ===");
	{
		RequestParser parser6;
		std::string request = "POST / HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n4\r\nTest\r\n1\r\n!\r\n5\r\nHello\r\n0\r\n\r\n";
		
		parser6.feed(request.c_str(), request.length());
		
		if (parser6.get_psr_state() == COMPLETE)
		{
			const HttpRequest& req = parser6.getRequest();
			Logger::info("✓ Body: '" + req.getBody() + "'");
			Logger::info("Expected: 'Test!Hello'");
		}
		else
		{
			Logger::error("✗ Expected COMPLETE but got: " + intToString(parser6.get_psr_state()));
		}
	}

	Logger::info("\n=== ESTADO DOS ENUMS (pra debug) ===");
	Logger::info("REQUEST_LINE = 0, HEADERS = 1, BODY = 2");
	Logger::info("CHUNK_SIZE = 3, CHUNK_DATA = 4, CHUNK_TRAILER = 5");
	Logger::info("COMPLETE = 6, ERROR = 7");
}

int main(int argc, char **argv)
{
	// Se passar "test" como argumento, roda testes
	if (argc > 1 && std::string(argv[1]) == "test")
	{
		testChunkedParser();
		return 0;
	}

	// Senão, roda servidor normal
	const std::string	host = "0.0.0.0";
	const int			port = 8081;
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
		Logger::info("Listening on 0.0.0.0:8081 — connect with: nc localhost 8081");
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



 /*
int main(void)
{
	const std::string	host = "0.0.0.0";
	const int			port = 8081;
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
}*/

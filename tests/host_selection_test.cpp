/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   host_selection_test.cpp                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: galves-a <galves-a@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/08/03 22:10:00 by galves-a          #+#    #+#             */
/*   Updated: 2026/08/03 22:10:00 by galves-a         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <iostream>
#include <string>
#include <vector>

#include "server/EventLoop.hpp"
#include "config/ServerConfig.hpp"
#include "http/HttpRequest.hpp"

static int	s_pass = 0;
static int	s_fail = 0;

# define TEST(cond, name) \
	do { \
		if (cond) { s_pass++; std::cout << "[PASS] " << name << std::endl; } \
		else { s_fail++; std::cerr << "[FAIL] " << name << std::endl; } \
	} while (0)

/**
 * @brief Builds a server block listening on `port` under a single name.
 * @param port The port the block listens on.
 * @param name The server_name of the block, stored lowercased as the loader does.
 * @param root The document root identifying the block in an assertion.
 * @return The populated server config.
 */
static ServerConfig	makeServer(int port, const std::string &name,
		const std::string &root)
{
	ServerConfig	config;

	config.port = port;
	config.root = root;
	if (!name.empty())
		config.serverNames.push_back(name);
	return (config);
}

/**
 * @brief Builds a server block bound to a specific interface, the way
 * conf/default.conf's "listen 127.0.0.1:8081;" style directives do.
 * @param host The exact address the block's listen directive names.
 * @param port The port the block listens on.
 * @param name The server_name of the block, or empty for none.
 * @param root The document root identifying the block in an assertion.
 * @return The populated server config.
 */
static ServerConfig	makeServerWithHost(const std::string &host, int port,
		const std::string &name, const std::string &root)
{
	ServerConfig	config = makeServer(port, name, root);

	config.host = host;
	return (config);
}

/**
 * @brief Builds a GET request carrying `host` as its Host header.
 * @param host The Host header value.
 * @return The populated request.
 */
static HttpRequest	makeRequest(const std::string &host)
{
	HttpRequest	request;

	request.setMethod("GET");
	request.setUri("/");
	request.setVersion("HTTP/1.1");
	request.setHeaders("Host", host);
	return (request);
}

/**
 * @brief Builds the two-block fixture both sharing port 8088.
 * @return The server blocks, portfolio first so it is also the default.
 */
static std::vector<ServerConfig>	makeConfigs(void)
{
	std::vector<ServerConfig>	configs;

	configs.push_back(makeServer(8088, "portfolio.com", "www/portfolio"));
	configs.push_back(makeServer(8088, "site-da-escola.com", "www/escola"));
	return (configs);
}

int	main(void)
{
	EventLoop	loop(makeConfigs());

	{
		const ServerConfig	&chosen = loop.getServerConfigForRequest(
				"0.0.0.0", 8088, makeRequest("portfolio.com"));

		TEST(chosen.root == "www/portfolio",
			"the Host header picks the block declaring it");
	}

	{
		const ServerConfig	&chosen = loop.getServerConfigForRequest(
				"0.0.0.0", 8088, makeRequest("site-da-escola.com"));

		TEST(chosen.root == "www/escola",
			"a Host other than the first block picks the right block");
	}

	{
		const ServerConfig	&chosen = loop.getServerConfigForRequest(
				"0.0.0.0", 8088, makeRequest("Site-Da-Escola.com"));

		TEST(chosen.root == "www/escola",
			"a mixed-case Host still picks the right block");
	}

	{
		const ServerConfig	&chosen = loop.getServerConfigForRequest(
				"0.0.0.0", 8088, makeRequest("SITE-DA-ESCOLA.COM"));

		TEST(chosen.root == "www/escola",
			"an uppercase Host still picks the right block");
	}

	{
		const ServerConfig	&chosen = loop.getServerConfigForRequest(
				"0.0.0.0", 8088, makeRequest("site-da-escola.com:8088"));

		TEST(chosen.root == "www/escola",
			"the port carried by the Host header is ignored when matching");
	}

	{
		const ServerConfig	&chosen = loop.getServerConfigForRequest(
				"0.0.0.0", 8088, makeRequest("Site-Da-Escola.com:8088"));

		TEST(chosen.root == "www/escola",
			"a mixed-case Host carrying a port still picks the right block");
	}

	{
		const ServerConfig	&chosen = loop.getServerConfigForRequest(
				"0.0.0.0", 8088, makeRequest("unknown.com"));

		TEST(chosen.root == "www/portfolio",
			"an unmatched Host falls back to the first block on the port");
	}

	{
		const ServerConfig	&chosen = loop.getServerConfigForRequest(
				"0.0.0.0", 8088, makeRequest(""));

		TEST(chosen.root == "www/portfolio",
			"an empty Host falls back to the first block on the port");
	}

	{
		std::vector<ServerConfig>	configs = makeConfigs();

		configs.push_back(makeServer(9090, "other.com", "www/other"));

		EventLoop			ported(configs);
		const ServerConfig	&chosen = ported.getServerConfigForRequest(
				"0.0.0.0", 9090, makeRequest("portfolio.com"));

		TEST(chosen.root == "www/other",
			"only blocks listening on the request port are considered");
	}

	{
		std::vector<ServerConfig>	configs;

		configs.push_back(makeServer(8088, "", "www/nameless"));
		configs.push_back(makeServer(8088, "named.com", "www/named"));

		EventLoop			nameless(configs);
		const ServerConfig	&chosen = nameless.getServerConfigForRequest(
				"0.0.0.0", 8088, makeRequest("named.com"));

		TEST(chosen.root == "www/named",
			"a block without server_name does not shadow a matching one");
	}

	{
		EventLoop	loop2(makeConfigs());

		TEST(loop2.cleanHostHeader("Example.COM:8080") == "example.com",
			"cleanHostHeader strips the port and lowercases the host");
	}

	/*
	 * conf/default.conf's site1 (127.0.0.1:8081) and site2 (127.0.0.2:8081):
	 * two blocks on the same port, each bound to its own interface. A request
	 * whose Host header does not literally match either server_name (the
	 * common case: curl defaults Host to whatever it dialled) must still be
	 * answered by the block bound to the interface the connection actually
	 * arrived on, not by whichever block happens to be declared first.
	 */
	{
		std::vector<ServerConfig>	configs;

		configs.push_back(makeServerWithHost("127.0.0.1", 8081, "site1",
					"www/site1"));
		configs.push_back(makeServerWithHost("127.0.0.2", 8081, "site2",
					"www/site2"));

		EventLoop	interfaces(configs);

		{
			const ServerConfig	&chosen = interfaces.getServerConfigForRequest(
					"127.0.0.1", 8081, makeRequest("127.0.0.1:8081"));

			TEST(chosen.root == "www/site1",
				"a connection on site1's interface is answered by site1 "
				"even when the Host header does not name it");
		}

		{
			const ServerConfig	&chosen = interfaces.getServerConfigForRequest(
					"127.0.0.2", 8081, makeRequest("127.0.0.2:8081"));

			TEST(chosen.root == "www/site2",
				"a connection on site2's interface is answered by site2, "
				"not by the first-declared block on the same port");
		}

		{
			const ServerConfig	&chosen = interfaces.getServerConfigForRequest(
					"127.0.0.2", 8081, makeRequest("site1"));

			TEST(chosen.root == "www/site2",
				"the interface the connection arrived on outranks a Host "
				"header that names a different interface's block");
		}
	}

	/*
	 * A wildcard block (bare "listen 8088;", host defaults to 0.0.0.0)
	 * shares its port with nothing else here, so any interface the
	 * connection lands on must still reach it.
	 */
	{
		std::vector<ServerConfig>	configs;

		configs.push_back(makeServer(8090, "any.com", "www/any"));

		EventLoop			wildcard(configs);
		const ServerConfig	&chosen = wildcard.getServerConfigForRequest(
				"127.0.0.5", 8090, makeRequest("any.com"));

		TEST(chosen.root == "www/any",
			"a wildcard-bound block is reached regardless of which local "
			"address the connection arrived on");
	}

	std::cout << std::endl << s_pass << " passed, " << s_fail << " failed"
		<< std::endl;
	return (s_fail == 0 ? 0 : 1);
}

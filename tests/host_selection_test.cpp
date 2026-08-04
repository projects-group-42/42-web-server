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
				8088, makeRequest("portfolio.com"));

		TEST(chosen.root == "www/portfolio",
			"the Host header picks the block declaring it");
	}

	{
		const ServerConfig	&chosen = loop.getServerConfigForRequest(
				8088, makeRequest("site-da-escola.com"));

		TEST(chosen.root == "www/escola",
			"a Host other than the first block picks the right block");
	}

	{
		const ServerConfig	&chosen = loop.getServerConfigForRequest(
				8088, makeRequest("Site-Da-Escola.com"));

		TEST(chosen.root == "www/escola",
			"a mixed-case Host still picks the right block");
	}

	{
		const ServerConfig	&chosen = loop.getServerConfigForRequest(
				8088, makeRequest("SITE-DA-ESCOLA.COM"));

		TEST(chosen.root == "www/escola",
			"an uppercase Host still picks the right block");
	}

	{
		const ServerConfig	&chosen = loop.getServerConfigForRequest(
				8088, makeRequest("site-da-escola.com:8088"));

		TEST(chosen.root == "www/escola",
			"the port carried by the Host header is ignored when matching");
	}

	{
		const ServerConfig	&chosen = loop.getServerConfigForRequest(
				8088, makeRequest("Site-Da-Escola.com:8088"));

		TEST(chosen.root == "www/escola",
			"a mixed-case Host carrying a port still picks the right block");
	}

	{
		const ServerConfig	&chosen = loop.getServerConfigForRequest(
				8088, makeRequest("unknown.com"));

		TEST(chosen.root == "www/portfolio",
			"an unmatched Host falls back to the first block on the port");
	}

	{
		const ServerConfig	&chosen = loop.getServerConfigForRequest(
				8088, makeRequest(""));

		TEST(chosen.root == "www/portfolio",
			"an empty Host falls back to the first block on the port");
	}

	{
		std::vector<ServerConfig>	configs = makeConfigs();

		configs.push_back(makeServer(9090, "other.com", "www/other"));

		EventLoop			ported(configs);
		const ServerConfig	&chosen = ported.getServerConfigForRequest(
				9090, makeRequest("portfolio.com"));

		TEST(chosen.root == "www/other",
			"only blocks listening on the request port are considered");
	}

	{
		std::vector<ServerConfig>	configs;

		configs.push_back(makeServer(8088, "", "www/nameless"));
		configs.push_back(makeServer(8088, "named.com", "www/named"));

		EventLoop			nameless(configs);
		const ServerConfig	&chosen = nameless.getServerConfigForRequest(
				8088, makeRequest("named.com"));

		TEST(chosen.root == "www/named",
			"a block without server_name does not shadow a matching one");
	}

	{
		EventLoop	loop2(makeConfigs());

		TEST(loop2.cleanHostHeader("Example.COM:8080") == "example.com",
			"cleanHostHeader strips the port and lowercases the host");
	}

	std::cout << std::endl << s_pass << " passed, " << s_fail << " failed"
		<< std::endl;
	return (s_fail == 0 ? 0 : 1);
}

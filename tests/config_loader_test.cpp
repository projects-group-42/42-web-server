/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   config_loader_test.cpp                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/08/01 18:00:00 by jucoelho          #+#    #+#             */
/*   Updated: 2026/08/01 18:00:00 by jucoelho         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

# include <cstdio>
# include <fstream>
# include <iostream>
# include <stdexcept>
# include <string>
# include <vector>

# include "config/ConfigLoader.hpp"

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

static const char	*TMP_PATH = "tests/.tmp_config_loader_test.conf";

static ServerConfig	loadSource(const std::string &source)
{
	std::ofstream	out(TMP_PATH);

	out << source;
	out.close();

	ConfigLoader				loader(TMP_PATH);
	std::vector<ServerConfig>	servers = loader.loader();

	std::remove(TMP_PATH);
	if (servers.empty())
		throw std::runtime_error("no server block parsed");
	return (servers[0]);
}

static std::vector<ServerConfig>	loadAll(const std::string &source)
{
	std::ofstream	out(TMP_PATH);

	out << source;
	out.close();

	ConfigLoader				loader(TMP_PATH);
	std::vector<ServerConfig>	servers = loader.loader();

	std::remove(TMP_PATH);
	return (servers);
}

static bool	loadThrows(const std::string &source)
{
	try
	{
		loadSource(source);
	}
	catch (const std::exception &)
	{
		std::remove(TMP_PATH);
		return (true);
	}
	return (false);
}

/* ------------------------------------------------------------------ */
/* Accepted listen forms                                               */
/* ------------------------------------------------------------------ */

static void	test_bare_port(void)
{
	ServerConfig	config = loadSource("server {\n    listen 8081;\n}\n");

	CHECK_EQ(config.port, 8081, "bare port is parsed");
	CHECK_EQ(config.host, std::string("0.0.0.0"),
	         "bare port keeps the default host");
}

static void	test_host_and_port(void)
{
	ServerConfig	config = loadSource("server {\n    listen 127.0.0.1:8082;\n}\n");

	CHECK_EQ(config.port, 8082, "host:port sets the port");
	CHECK_EQ(config.host, std::string("127.0.0.1"), "host:port sets the host");
}

static void	test_localhost_is_resolved(void)
{
	ServerConfig	config = loadSource("server {\n    listen localhost:8084;\n}\n");

	CHECK_EQ(config.host, std::string("127.0.0.1"),
	         "localhost resolves to 127.0.0.1");
	CHECK_EQ(config.port, 8084, "localhost:port sets the port");
}

static void	test_wildcard_host(void)
{
	ServerConfig	config = loadSource("server {\n    listen 0.0.0.0:8085;\n}\n");

	CHECK_EQ(config.host, std::string("0.0.0.0"), "0.0.0.0 is accepted");
	CHECK_EQ(config.port, 8085, "0.0.0.0:port sets the port");
}

static void	test_boundary_ports(void)
{
	CHECK_EQ(loadSource("server {\n    listen 1;\n}\n").port, 1,
	         "port 1 is accepted");
	CHECK_EQ(loadSource("server {\n    listen 65535;\n}\n").port, 65535,
	         "port 65535 is accepted");
}

/* ------------------------------------------------------------------ */
/* Rejected listen forms                                               */
/* ------------------------------------------------------------------ */

static void	test_port_out_of_range_throws(void)
{
	TEST(loadThrows("server {\n    listen 0;\n}\n"),
	     "port 0 is rejected");
	TEST(loadThrows("server {\n    listen 65536;\n}\n"),
	     "port 65536 is rejected");
	TEST(loadThrows("server {\n    listen 99999999999999;\n}\n"),
	     "an overflowing port is rejected");
}

static void	test_non_numeric_port_throws(void)
{
	TEST(loadThrows("server {\n    listen abc;\n}\n"),
	     "a non-numeric port is rejected");
}

static void	test_trailing_garbage_port_throws(void)
{
	TEST(loadThrows("server {\n    listen 8080abc;\n}\n"),
	     "'8080abc' is rejected instead of truncating to 8080");
	TEST(loadThrows("server {\n    listen 80.5;\n}\n"),
	     "'80.5' is rejected instead of truncating to 80");
}

static void	test_extra_arguments_throw(void)
{
	TEST(loadThrows("server {\n    listen 8080 extra_arg;\n}\n"),
	     "extra listen arguments are rejected");
}

static void	test_malformed_host_port_throws(void)
{
	TEST(loadThrows("server {\n    listen 0.0.0.0:8085:9;\n}\n"),
	     "a second ':' is rejected");
	TEST(loadThrows("server {\n    listen :8083;\n}\n"),
	     "an empty host is rejected");
	TEST(loadThrows("server {\n    listen 127.0.0.1:;\n}\n"),
	     "an empty port is rejected");
}

static void	test_unresolvable_host_throws(void)
{
	TEST(loadThrows("server {\n    listen example.com:80;\n}\n"),
	     "a hostname inet_pton cannot parse is rejected");
	TEST(loadThrows("server {\n    listen 999.1.1.1:80;\n}\n"),
	     "an out-of-range IPv4 octet is rejected");
}

/* ------------------------------------------------------------------ */
/* Loader behaviour                                                    */
/* ------------------------------------------------------------------ */

static void	test_missing_file_throws(void)
{
	ConfigLoader	loader("tests/.this_file_does_not_exist.conf");
	bool			threw = false;

	try
	{
		loader.loader();
	}
	catch (const std::exception &)
	{
		threw = true;
	}
	TEST(threw, "a missing config file throws instead of falling back silently");
}

static void	test_no_listen_uses_defaults(void)
{
	ServerConfig	config = loadSource("server {\n    root www/;\n}\n");

	CHECK_EQ(config.port, 8080, "a config without listen keeps the default port");
	CHECK_EQ(config.host, std::string("0.0.0.0"),
	         "a config without listen keeps the default host");
}

static void	test_last_listen_wins(void)
{
	ServerConfig	config = loadSource(
		"server {\n    listen 8081;\n    listen 8082;\n}\n");

	CHECK_EQ(config.port, 8082,
	         "the last listen of a server block wins for now");
}

static void	test_each_server_block_is_kept(void)
{
	std::vector<ServerConfig>	servers = loadAll(
		"server {\n    listen 8081;\n}\nserver {\n    listen 8082;\n}\n");

	CHECK_EQ(servers.size(), static_cast<size_t>(2),
	         "every server block yields its own config");
	if (servers.size() != 2)
		return ;
	CHECK_EQ(servers[0].port, 8081, "the first server keeps its own port");
	CHECK_EQ(servers[1].port, 8082, "the second server keeps its own port");
}

static void	test_no_server_block_throws(void)
{
	TEST(loadThrows("listen 8081;\n"),
	     "a config without a server block throws");
}

/* ------------------------------------------------------------------ */
/* root                                                                */
/* ------------------------------------------------------------------ */

static void	test_server_root(void)
{
	ServerConfig	config = loadSource(
		"server {\n    listen 8081;\n    root www;\n}\n");

	CHECK_EQ(config.root, std::string("www"),
	         "a server level root is parsed");
}

static void	test_root_trailing_slash_is_trimmed(void)
{
	ServerConfig	config = loadSource(
		"server {\n    listen 8081;\n    root www/;\n}\n");

	CHECK_EQ(config.root, std::string("www"),
	         "a trailing slash is stripped from root");
}

static void	test_root_defaults_when_absent(void)
{
	ServerConfig	config = loadSource("server {\n    listen 8081;\n}\n");

	CHECK_EQ(config.root, std::string(DEFAULT_ROOT),
	         "a server without root falls back to the default root");
}

static void	test_location_root_is_parsed(void)
{
	ServerConfig	config = loadSource(
		"server {\n    listen 8081;\n    root www;\n"
		"    location /images/ {\n        root www/assets;\n    }\n}\n");

	CHECK_EQ(config.locations.size(), static_cast<size_t>(1),
	         "the location block is kept");
	if (config.locations.empty())
		return ;
	CHECK_EQ(config.locations[0].path, std::string("/images/"),
	         "the location path is kept");
	CHECK_EQ(config.locations[0].root, std::string("www/assets"),
	         "a location level root is parsed");
}

static void	test_location_without_root_inherits(void)
{
	ServerConfig	config = loadSource(
		"server {\n    listen 8081;\n    root www;\n"
		"    location /images/ {\n        autoindex on;\n    }\n}\n");

	CHECK_EQ(config.locations.size(), static_cast<size_t>(1),
	         "the location block is kept");
	if (config.locations.empty())
		return ;
	TEST(config.locations[0].root.empty(),
	     "a location without root stays empty so it inherits the server root");
	TEST(config.locations[0].autoindex,
	     "autoindex is still parsed alongside root");
}

static void	test_root_without_argument_throws(void)
{
	TEST(loadThrows("server {\n    listen 8081;\n    root;\n}\n"),
	     "root without an argument throws");
}

static void	test_root_with_extra_arguments_throws(void)
{
	TEST(loadThrows("server {\n    listen 8081;\n    root www extra;\n}\n"),
	     "root with more than one argument throws");
}

static void	test_static_helpers(void)
{
	CHECK_EQ(ConfigLoader::parsePort("8080"), 8080,
	         "parsePort accepts a plain port");
	CHECK_EQ(ConfigLoader::parseHost("localhost"), std::string("127.0.0.1"),
	         "parseHost maps localhost");
	CHECK_EQ(ConfigLoader::parseHost("10.0.0.1"), std::string("10.0.0.1"),
	         "parseHost passes an IPv4 literal through");
}

int	main(void)
{
	test_bare_port();
	test_host_and_port();
	test_localhost_is_resolved();
	test_wildcard_host();
	test_boundary_ports();
	test_port_out_of_range_throws();
	test_non_numeric_port_throws();
	test_trailing_garbage_port_throws();
	test_extra_arguments_throw();
	test_malformed_host_port_throws();
	test_unresolvable_host_throws();
	test_missing_file_throws();
	test_no_listen_uses_defaults();
	test_last_listen_wins();
	test_each_server_block_is_kept();
	test_no_server_block_throws();
	test_server_root();
	test_root_trailing_slash_is_trimmed();
	test_root_defaults_when_absent();
	test_location_root_is_parsed();
	test_location_without_root_inherits();
	test_root_without_argument_throws();
	test_root_with_extra_arguments_throws();
	test_static_helpers();

	std::cout << std::endl;
	std::cout << s_pass << " passed, " << s_fail << " failed, "
	          << (s_pass + s_fail) << " total" << std::endl;

	return (s_fail == 0 ? 0 : 1);
}

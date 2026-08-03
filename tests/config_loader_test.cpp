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

	ConfigLoader	loader(TMP_PATH);
	ServerConfig	config = loader.loader();

	std::remove(TMP_PATH);
	return (config);
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
/* index directive                                                     */
/* ------------------------------------------------------------------ */

static void	test_default_index(void)
{
	ServerConfig	config = loadSource(
		"server {\n    listen 8081;\n    location / {\n    }\n}\n");

	CHECK_EQ(config.index, std::string("index.html"),
	         "a config without index keeps the default index");
	CHECK_EQ(config.locations.size(), static_cast<size_t>(1),
	         "the location block is loaded");
	CHECK_EQ(config.locations[0].index, std::string("index.html"),
	         "a location without index inherits the default index");
}

static void	test_server_index_is_parsed(void)
{
	ServerConfig	config = loadSource(
		"server {\n    listen 8081;\n    index custom.html;\n}\n");

	CHECK_EQ(config.index, std::string("custom.html"),
	         "a server-level index is parsed");
}

static void	test_location_inherits_server_index(void)
{
	ServerConfig	config = loadSource(
		"server {\n    listen 8081;\n    index custom.html;\n"
		"    location / {\n        autoindex off;\n    }\n}\n");

	CHECK_EQ(config.locations.size(), static_cast<size_t>(1),
	         "the location block is loaded");
	CHECK_EQ(config.locations[0].index, std::string("custom.html"),
	         "a location without index inherits the server index");
}

static void	test_location_index_overrides_server(void)
{
	ServerConfig	config = loadSource(
		"server {\n    listen 8081;\n    index custom.html;\n"
		"    location / {\n        index location.html;\n    }\n}\n");

	CHECK_EQ(config.index, std::string("custom.html"),
	         "the server index is left untouched by the location");
	CHECK_EQ(config.locations[0].index, std::string("location.html"),
	         "a location index overrides the server index");
}

static void	test_last_server_index_wins(void)
{
	ServerConfig	config = loadSource(
		"server {\n    listen 8081;\n    index first.html;\n"
		"    index second.html;\n}\n");

	CHECK_EQ(config.index, std::string("second.html"),
	         "the last server index directive wins");
}

static void	test_index_is_shared_across_servers(void)
{
	ServerConfig	config = loadSource(
		"server {\n    listen 8081;\n    index first.html;\n"
		"    location /x {\n    }\n}\n"
		"server {\n    listen 8082;\n    index second.html;\n"
		"    location /y {\n    }\n}\n");

	CHECK_EQ(config.locations.size(), static_cast<size_t>(2),
	         "locations of every server block are loaded");
	CHECK_EQ(config.locations[0].index, std::string("second.html"),
	         "only one server is supported, so a location of the first server "
	         "still inherits the last index");
	CHECK_EQ(config.locations[1].index, std::string("second.html"),
	         "a location of the last server inherits its index");
}

static void	test_malformed_index_throws(void)
{
	TEST(loadThrows("server {\n    index a.html b.html;\n}\n"),
	     "an index list is rejected instead of keeping only the first entry");
	TEST(loadThrows("server {\n    index;\n}\n"),
	     "an index without an argument is rejected");
	TEST(loadThrows(
		"server {\n    location / {\n        index a.html b.html;\n    }\n}\n"),
	     "an index list inside a location is rejected");
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
		"server {\n    listen 8081;\n}\nserver {\n    listen 8082;\n}\n");

	CHECK_EQ(config.port, 8082, "the last listen directive wins for now");
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
	test_default_index();
	test_server_index_is_parsed();
	test_location_inherits_server_index();
	test_location_index_overrides_server();
	test_last_server_index_wins();
	test_index_is_shared_across_servers();
	test_malformed_index_throws();
	test_missing_file_throws();
	test_no_listen_uses_defaults();
	test_last_listen_wins();
	test_static_helpers();

	std::cout << std::endl;
	std::cout << s_pass << " passed, " << s_fail << " failed, "
	          << (s_pass + s_fail) << " total" << std::endl;

	return (s_fail == 0 ? 0 : 1);
}

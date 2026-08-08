/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   config_loader_test.cpp                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/08/01 18:00:00 by jucoelho          #+#    #+#             */
/*   Updated: 2026/08/08 19:56:13 by jucoelho         ###   ########.fr       */
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

static void	test_index_is_per_server(void)
{
	std::vector<ServerConfig>	servers = loadAll(
		"server {\n    listen 8081;\n    index first.html;\n"
		"    location /x {\n    }\n}\n"
		"server {\n    listen 8082;\n    index second.html;\n"
		"    location /y {\n    }\n}\n");

	CHECK_EQ(servers.size(), static_cast<size_t>(2),
	         "every server block yields its own config");
	if (servers.size() != 2)
		return ;
	CHECK_EQ(servers[0].index, std::string("first.html"),
	         "the first server keeps its own index");
	CHECK_EQ(servers[1].index, std::string("second.html"),
	         "the second server keeps its own index");
	CHECK_EQ(servers[0].locations.size(), static_cast<size_t>(1),
	         "a server only owns the locations declared inside it");
	CHECK_EQ(servers[0].locations[0].index, std::string("first.html"),
	         "a location inherits the index of its own server");
	CHECK_EQ(servers[1].locations[0].index, std::string("second.html"),
	         "a location of the second server inherits its own server index");
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
/* limit_except                                                        */
/* ------------------------------------------------------------------ */

static void	test_location_without_limit_except_is_unrestricted(void)
{
	ServerConfig	server = loadSource(
		"server {\n    location / {\n        index a.html;\n    }\n}\n");

	CHECK_EQ(server.locations.size(), static_cast<size_t>(1),
	         "the location is parsed");
	CHECK_EQ(server.locations[0].allowedMethods.size(),
	         static_cast<size_t>(0),
	         "a location without limit_except restricts no method");
}

static void	test_limit_except_is_parsed(void)
{
	ServerConfig	server = loadSource(
		"server {\n    location / {\n        limit_except GET POST;\n"
		"    }\n}\n");

	CHECK_EQ(server.locations[0].allowedMethods.size(),
	         static_cast<size_t>(2), "both methods are kept");
	CHECK_EQ(server.locations[0].allowedMethods[0], std::string("GET"),
	         "the first method is GET");
	CHECK_EQ(server.locations[0].allowedMethods[1], std::string("POST"),
	         "the second method is POST");
}

static void	test_limit_except_stores_a_repeat_once(void)
{
	ServerConfig	server = loadSource(
		"server {\n    location / {\n        limit_except GET GET POST;\n"
		"    }\n}\n");

	CHECK_EQ(server.locations[0].allowedMethods.size(),
	         static_cast<size_t>(2),
	         "a method named twice is stored once, so Allow never repeats it");
}

static void	test_limit_except_is_per_location(void)
{
	ServerConfig	server = loadSource(
		"server {\n"
		"    location / {\n        limit_except GET;\n    }\n"
		"    location /uploads {\n        limit_except GET POST DELETE;\n"
		"    }\n}\n");

	CHECK_EQ(server.locations[0].allowedMethods.size(),
	         static_cast<size_t>(1), "the first location keeps its own list");
	CHECK_EQ(server.locations[1].allowedMethods.size(),
	         static_cast<size_t>(3), "the second location keeps its own list");
}

static void	test_malformed_limit_except_throws(void)
{
	TEST(loadThrows(
		"server {\n    location / {\n        limit_except;\n    }\n}\n"),
	     "a limit_except without a method is rejected");
	TEST(loadThrows(
		"server {\n    location / {\n        limit_except PUT;\n    }\n}\n"),
	     "an unimplemented method is rejected instead of being dropped, which "
	     "would leave the list empty and the location open");
	TEST(loadThrows(
		"server {\n    location / {\n        limit_except GET post;\n"
		"    }\n}\n"),
	     "a lowercase method is rejected rather than silently ignored");
}

/* ------------------------------------------------------------------ */
/* upload_store                                                        */
/* ------------------------------------------------------------------ */

static void	test_location_without_upload_store_is_empty(void)
{
	ServerConfig	server = loadSource(
		"server {\n    location / {\n        index a.html;\n    }\n}\n");

	CHECK_EQ(server.locations.size(), static_cast<size_t>(1),
	         "the location is parsed");
	CHECK_EQ(server.locations[0].uploadStore, std::string(""),
	         "a location without upload_store keeps no upload directory");
}

static void	test_upload_store_is_parsed(void)
{
	ServerConfig	server = loadSource(
		"server {\n    location /uploads {\n"
		"        upload_store www/uploads;\n    }\n}\n");

	CHECK_EQ(server.locations[0].uploadStore, std::string("www/uploads"),
	         "the upload directory is kept");
}

static void	test_upload_store_trailing_slash_is_trimmed(void)
{
	ServerConfig	server = loadSource(
		"server {\n    location /uploads {\n"
		"        upload_store www/uploads/;\n    }\n}\n");

	CHECK_EQ(server.locations[0].uploadStore, std::string("www/uploads"),
	         "a trailing slash is stripped, as it is for root");
}

static void	test_upload_store_is_per_location(void)
{
	ServerConfig	server = loadSource(
		"server {\n"
		"    location / {\n        index a.html;\n    }\n"
		"    location /uploads {\n        upload_store www/uploads;\n"
		"    }\n}\n");

	CHECK_EQ(server.locations[0].uploadStore, std::string(""),
	         "a location without the directive is left untouched by another");
	CHECK_EQ(server.locations[1].uploadStore, std::string("www/uploads"),
	         "the declaring location keeps its own upload directory");
}

static void	test_last_upload_store_wins(void)
{
	ServerConfig	server = loadSource(
		"server {\n    location /uploads {\n"
		"        upload_store www/first;\n"
		"        upload_store www/second;\n    }\n}\n");

	CHECK_EQ(server.locations[0].uploadStore, std::string("www/second"),
	         "a directive declared twice keeps the last path");
}

static void	test_malformed_upload_store_throws(void)
{
	TEST(loadThrows(
		"server {\n    location /uploads {\n        upload_store;\n"
		"    }\n}\n"),
	     "an upload_store without a path is rejected");
	TEST(loadThrows(
		"server {\n    location /uploads {\n"
		"        upload_store www/uploads extra;\n    }\n}\n"),
	     "an extra argument is rejected instead of being silently dropped");
}

static void	test_unknown_server_directive_throws(void)
{
	TEST(loadThrows(
		"server {\n    listen 8081;\n    unsupported_directive on;\n}\n"),
	     "an unknown server-level directive is rejected");
}

static void	test_unknown_location_directive_throws(void)
{
	TEST(loadThrows(
		"server {\n    listen 8081;\n    location / {\n        unsupported_directive on;\n    }\n}\n"),
	     "an unknown location-level directive is rejected");
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

/**
 * @brief A single server_name yields one stored name.
 */
static void	test_single_server_name(void)
{
	ServerConfig	config = loadSource(
		"server {\n    listen 8080;\n    server_name example.com;\n}\n");

	CHECK_EQ(config.serverNames.size(), static_cast<size_t>(1),
	         "a single server_name yields one entry");
	CHECK_EQ(config.serverNames[0], std::string("example.com"),
	         "the declared name is stored");
}

/**
 * @brief One directive may carry several names.
 */
static void	test_several_names_in_one_directive(void)
{
	ServerConfig	config = loadSource(
		"server {\n    server_name example.com www.example.com;\n}\n");

	CHECK_EQ(config.serverNames.size(), static_cast<size_t>(2),
	         "server_name accepts several names in one directive");
	CHECK_EQ(config.serverNames[1], std::string("www.example.com"),
	         "every name of the directive is stored");
}

/**
 * @brief Repeated directives add to the list instead of replacing it.
 */
static void	test_repeated_server_name_directives_accumulate(void)
{
	ServerConfig	config = loadSource(
		"server {\n    server_name a.com;\n    server_name b.com;\n}\n");

	CHECK_EQ(config.serverNames.size(), static_cast<size_t>(2),
	         "repeated server_name directives accumulate");
}

/**
 * @brief Names are stored lowercased so the Host match is case-insensitive.
 */
static void	test_server_name_is_lowercased(void)
{
	ServerConfig	config = loadSource(
		"server {\n    server_name EXAMPLE.COM;\n}\n");

	CHECK_EQ(config.serverNames[0], std::string("example.com"),
	         "a server_name is stored lowercased");
}

/**
 * @brief A server without the directive keeps an empty name list.
 */
static void	test_no_server_name_leaves_list_empty(void)
{
	ServerConfig	config = loadSource("server {\n    listen 8080;\n}\n");

	CHECK_EQ(config.serverNames.size(), static_cast<size_t>(0),
	         "a server without server_name keeps an empty name list");
}

/**
 * @brief Each server block keeps the names it declared.
 */
static void	test_server_names_are_per_server(void)
{
	std::vector<ServerConfig>	servers = loadAll(
		"server {\n    listen 8080;\n    server_name first.com;\n}\n"
		"server {\n    listen 8081;\n    server_name second.com;\n}\n");

	CHECK_EQ(servers.size(), static_cast<size_t>(2),
	         "both server blocks are kept");
	CHECK_EQ(servers[0].serverNames[0], std::string("first.com"),
	         "the first block keeps its own name");
	CHECK_EQ(servers[1].serverNames[0], std::string("second.com"),
	         "the second block keeps its own name");
}

/**
 * @brief A server_name carrying no name is rejected.
 */
static void	test_server_name_without_argument_throws(void)
{
	TEST(loadThrows("server {\n    listen 8080;\n    server_name;\n}\n"),
	     "server_name without an argument throws");
}

/**
 * @brief Without the directive the server keeps the 1 MiB default.
 */
static void	test_default_body_size(void)
{
	ServerConfig	config = loadSource("server {\n    listen 8080;\n}\n");

	CHECK_EQ(config.clientMaxBodySize, 1L * 1024L * 1024L,
	         "a server without client_max_body_size keeps the 1 MiB default");
}

/**
 * @brief A plain number is read as a byte count.
 */
static void	test_body_size_in_bytes(void)
{
	ServerConfig	config = loadSource(
		"server {\n    client_max_body_size 4096;\n}\n");

	CHECK_EQ(config.clientMaxBodySize, 4096L,
	         "a bare client_max_body_size is a byte count");
}

/**
 * @brief The k, m and g suffixes scale the value, in either case.
 */
static void	test_body_size_suffixes(void)
{
	CHECK_EQ(loadSource(
		"server {\n    client_max_body_size 10M;\n}\n").clientMaxBodySize,
		10L * 1024L * 1024L, "the M suffix means mebibytes");
	CHECK_EQ(loadSource(
		"server {\n    client_max_body_size 512k;\n}\n").clientMaxBodySize,
		512L * 1024L, "the k suffix means kibibytes");
	CHECK_EQ(loadSource(
		"server {\n    client_max_body_size 1g;\n}\n").clientMaxBodySize,
		1024L * 1024L * 1024L, "the g suffix means gibibytes");
	CHECK_EQ(loadSource(
		"server {\n    client_max_body_size 10m;\n}\n").clientMaxBodySize,
		10L * 1024L * 1024L, "a lowercase suffix is accepted too");
}

/**
 * @brief A location declaring a size of its own keeps it.
 */
static void	test_location_body_size_overrides_server(void)
{
	ServerConfig	config = loadSource(
		"server {\n"
		"    client_max_body_size 10M;\n"
		"    location /upload {\n        client_max_body_size 1M;\n    }\n"
		"}\n");

	CHECK_EQ(config.locations.size(), static_cast<size_t>(1),
	         "the location block is parsed");
	CHECK_EQ(config.locations[0].clientMaxBodySize, 1L * 1024L * 1024L,
	         "a location client_max_body_size is parsed");
}

/**
 * @brief A location declaring no size keeps the sentinel so it can inherit.
 */
static void	test_location_without_body_size_inherits(void)
{
	ServerConfig	config = loadSource(
		"server {\n"
		"    client_max_body_size 10M;\n"
		"    location /upload {\n        autoindex on;\n    }\n"
		"}\n");

	CHECK_EQ(config.locations[0].clientMaxBodySize, -1L,
	         "a location without client_max_body_size keeps the -1 sentinel");
}

/**
 * @brief Malformed sizes are rejected instead of silently defaulting.
 */
static void	test_malformed_body_size_throws(void)
{
	TEST(loadThrows("server {\n    client_max_body_size;\n}\n"),
	     "client_max_body_size without an argument throws");
	TEST(loadThrows("server {\n    client_max_body_size 10M 20M;\n}\n"),
	     "client_max_body_size with more than one argument throws");
	TEST(loadThrows("server {\n    client_max_body_size ten;\n}\n"),
	     "a non-numeric client_max_body_size throws");
	TEST(loadThrows("server {\n    client_max_body_size 10X;\n}\n"),
	     "an unknown client_max_body_size suffix throws");
	TEST(loadThrows("server {\n    client_max_body_size -1;\n}\n"),
	     "a negative client_max_body_size throws");
	TEST(loadThrows(
		"server {\n    client_max_body_size 999999999999999999999;\n}\n"),
	     "an out-of-range client_max_body_size throws");
}

/**
 * @brief A location without cgi_pass binds no interpreter at all.
 */
static void	test_location_without_cgi_pass_is_empty(void)
{
	ServerConfig	config = loadSource(
		"server {\n"
		"    location /cgi {\n        autoindex on;\n    }\n"
		"}\n");

	CHECK_EQ(config.locations.size(), static_cast<size_t>(1),
	         "the location block is parsed");
	if (config.locations.empty())
		return ;
	TEST(config.locations[0].cgiPass.empty(),
	     "a location without cgi_pass binds no interpreter");
}

/**
 * @brief A cgi_pass directive binds its extension to its interpreter.
 */
static void	test_cgi_pass_binds_python(void)
{
	ServerConfig	config = loadSource(
		"server {\n"
		"    location /cgi {\n"
		"        cgi_pass .py /usr/bin/python3;\n    }\n"
		"}\n");

	CHECK_EQ(config.locations.size(), static_cast<size_t>(1),
	         "the location block is parsed");
	if (config.locations.empty())
		return ;
	CHECK_EQ(config.locations[0].cgiPass.size(), static_cast<size_t>(1),
	         "a single cgi_pass binds a single extension");
	CHECK_EQ(config.locations[0].cgiPass[".py"],
	         std::string("/usr/bin/python3"),
	         "cgi_pass binds .py to the python interpreter");
}

/**
 * @brief Several cgi_pass directives coexist in the same location.
 */
static void	test_cgi_pass_accumulates_extensions(void)
{
	ServerConfig	config = loadSource(
		"server {\n"
		"    location /cgi {\n"
		"        cgi_pass .py /usr/bin/python3;\n"
		"        cgi_pass .pl /usr/bin/perl;\n    }\n"
		"}\n");

	if (config.locations.empty())
		return ;
	CHECK_EQ(config.locations[0].cgiPass.size(), static_cast<size_t>(2),
	         "both cgi_pass directives are kept");
	CHECK_EQ(config.locations[0].cgiPass[".py"],
	         std::string("/usr/bin/python3"),
	         "the python interpreter is kept");
	CHECK_EQ(config.locations[0].cgiPass[".pl"], std::string("/usr/bin/perl"),
	         "the perl interpreter is kept");
}

/**
 * @brief A cgi_pass directive binds .php to the php-cgi handler.
 */
static void	test_cgi_pass_binds_php(void)
{
	ServerConfig	config = loadSource(
		"server {\n"
		"    location /cgi {\n"
		"        cgi_pass .py /usr/bin/python3;\n"
		"        cgi_pass .php /usr/bin/php-cgi;\n    }\n"
		"}\n");

	CHECK_EQ(config.locations.size(), static_cast<size_t>(1),
	         "the location block is parsed");
	if (config.locations.empty())
		return ;
	CHECK_EQ(config.locations[0].cgiPass[".php"],
	         std::string("/usr/bin/php-cgi"),
	         "cgi_pass binds .php to the php-cgi handler");
	CHECK_EQ(config.locations[0].cgiPass[".py"],
	         std::string("/usr/bin/python3"),
	         "the python binding survives alongside the php one");
}

/**
 * @brief An extension declared twice keeps the interpreter declared last.
 */
static void	test_last_cgi_pass_wins(void)
{
	ServerConfig	config = loadSource(
		"server {\n"
		"    location /cgi {\n"
		"        cgi_pass .py /usr/bin/python2;\n"
		"        cgi_pass .py /usr/bin/python3;\n    }\n"
		"}\n");

	if (config.locations.empty())
		return ;
	CHECK_EQ(config.locations[0].cgiPass.size(), static_cast<size_t>(1),
	         "a repeated extension is stored once");
	CHECK_EQ(config.locations[0].cgiPass[".py"],
	         std::string("/usr/bin/python3"),
	         "the last cgi_pass declaration wins");
}

/**
 * @brief cgi_pass is per location, so one location never sees another's.
 */
static void	test_cgi_pass_is_per_location(void)
{
	ServerConfig	config = loadSource(
		"server {\n"
		"    location / {\n        autoindex on;\n    }\n"
		"    location /cgi {\n"
		"        cgi_pass .py /usr/bin/python3;\n    }\n"
		"}\n");

	CHECK_EQ(config.locations.size(), static_cast<size_t>(2),
	         "both location blocks are parsed");
	if (config.locations.size() != 2)
		return ;
	TEST(config.locations[0].cgiPass.empty(),
	     "the location without cgi_pass stays empty");
	CHECK_EQ(config.locations[1].cgiPass.size(), static_cast<size_t>(1),
	         "the location declaring cgi_pass keeps it");
}

static void	test_malformed_cgi_pass_throws(void)
{
	TEST(loadThrows("server {\n    location /cgi {\n        cgi_pass;\n"
		"    }\n}\n"),
	     "cgi_pass without arguments throws");
	TEST(loadThrows("server {\n    location /cgi {\n        cgi_pass .py;\n"
		"    }\n}\n"),
	     "cgi_pass without an interpreter throws");
	TEST(loadThrows("server {\n    location /cgi {\n"
		"        cgi_pass .py /usr/bin/python3 extra;\n    }\n}\n"),
	     "cgi_pass with more than two arguments throws");
	TEST(loadThrows("server {\n    location /cgi {\n"
		"        cgi_pass py /usr/bin/python3;\n    }\n}\n"),
	     "a cgi_pass extension without a leading dot throws");
	TEST(loadThrows("server {\n    location /cgi {\n"
		"        cgi_pass . /usr/bin/python3;\n    }\n}\n"),
	     "a cgi_pass extension reduced to a dot throws");
}

/* ------------------------------------------------------------------ */
/* return (redirects)                                                  */
/* ------------------------------------------------------------------ */

/**
 * @brief A location declaring no return keeps the 0 sentinel.
 */
static void	test_location_without_return_is_zero(void)
{
	ServerConfig	config = loadSource(
		"server {\n    location / {\n        autoindex on;\n    }\n}\n");

	CHECK_EQ(config.locations.size(), static_cast<size_t>(1),
	         "the location block is parsed");
	if (config.locations.empty())
		return ;
	CHECK_EQ(config.locations[0].returnCode, 0,
	         "a location without return keeps the 0 sentinel");
	TEST(config.locations[0].returnUrl.empty(),
	     "a location without return keeps an empty target");
}

/**
 * @brief Both redirect codes the response builder knows are accepted.
 */
static void	test_return_codes_are_parsed(void)
{
	ServerConfig	moved = loadSource(
		"server {\n    location /old {\n        return 301 /new;\n    }\n}\n");
	ServerConfig	found = loadSource(
		"server {\n    location /old {\n        return 302 /new;\n    }\n}\n");

	if (moved.locations.empty() || found.locations.empty())
	{
		TEST(false, "both return blocks are parsed");
		return ;
	}
	CHECK_EQ(moved.locations[0].returnCode, 301, "return 301 is parsed");
	CHECK_EQ(moved.locations[0].returnUrl, std::string("/new"),
	         "return 301 keeps its target");
	CHECK_EQ(found.locations[0].returnCode, 302, "return 302 is parsed");
}

/**
 * @brief An absolute URL is a valid redirect target, not only a local path.
 */
static void	test_return_accepts_absolute_url(void)
{
	ServerConfig	config = loadSource(
		"server {\n    location /old {\n"
		"        return 301 http://example.com/new;\n    }\n}\n");

	if (config.locations.empty())
	{
		TEST(false, "the return block is parsed");
		return ;
	}
	CHECK_EQ(config.locations[0].returnUrl,
	         std::string("http://example.com/new"),
	         "an absolute URL is kept as the target");
}

/**
 * @brief return is per location, so one location never sees another's.
 */
static void	test_return_is_per_location(void)
{
	ServerConfig	config = loadSource(
		"server {\n"
		"    location / {\n        autoindex on;\n    }\n"
		"    location /old {\n        return 301 /new;\n    }\n"
		"}\n");

	CHECK_EQ(config.locations.size(), static_cast<size_t>(2),
	         "both location blocks are parsed");
	if (config.locations.size() != 2)
		return ;
	CHECK_EQ(config.locations[0].returnCode, 0,
	         "the location without return stays at 0");
	CHECK_EQ(config.locations[1].returnCode, 301,
	         "the location declaring return keeps it");
}

/**
 * @brief The last return declaration wins, as the other directives do.
 */
static void	test_last_return_wins(void)
{
	ServerConfig	config = loadSource(
		"server {\n    location /old {\n"
		"        return 301 /first;\n        return 302 /second;\n    }\n}\n");

	if (config.locations.empty())
	{
		TEST(false, "the return block is parsed");
		return ;
	}
	CHECK_EQ(config.locations[0].returnCode, 302,
	         "the last return code wins");
	CHECK_EQ(config.locations[0].returnUrl, std::string("/second"),
	         "the last return target wins");
}

/**
 * @brief A malformed return is refused at load time.
 * A status code that never reaches a reason phrase would be serialised as an
 * invalid status line, so it is refused while the config is read instead.
 */
static void	test_malformed_return_throws(void)
{
	TEST(loadThrows("server {\n    location /old {\n        return;\n"
		"    }\n}\n"),
	     "return without arguments throws");
	TEST(loadThrows("server {\n    location /old {\n        return 301;\n"
		"    }\n}\n"),
	     "return without a target throws");
	TEST(loadThrows("server {\n    location /old {\n"
		"        return 301 /a /b;\n    }\n}\n"),
	     "return with more than two arguments throws");
	TEST(loadThrows("server {\n    location /old {\n"
		"        return abc /new;\n    }\n}\n"),
	     "a non-numeric status code is refused");
	TEST(loadThrows("server {\n    location /old {\n"
		"        return -301 /new;\n    }\n}\n"),
	     "a negative status code is refused");
	TEST(loadThrows("server {\n    location /old {\n"
		"        return 999 /new;\n    }\n}\n"),
	     "a status code outside 301/302 is refused");
	TEST(loadThrows("server {\n    location /old {\n"
		"        return 200 /new;\n    }\n}\n"),
	     "a non-redirect status code is refused");
	TEST(loadThrows("server {\n    location /old {\n"
		"        return 3011 /new;\n    }\n}\n"),
	     "a status code longer than three digits is refused");
	TEST(loadThrows("server {\n    location /old {\n"
		"        return 301 relative;\n    }\n}\n"),
	     "a target that is neither an absolute path nor a URL is refused");
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
	test_index_is_per_server();
	test_malformed_index_throws();

	test_location_without_limit_except_is_unrestricted();
	test_limit_except_is_parsed();
	test_limit_except_stores_a_repeat_once();
	test_limit_except_is_per_location();
	test_malformed_limit_except_throws();
	test_location_without_upload_store_is_empty();
	test_upload_store_is_parsed();
	test_upload_store_trailing_slash_is_trimmed();
	test_upload_store_is_per_location();
	test_last_upload_store_wins();
	test_malformed_upload_store_throws();
	test_unknown_server_directive_throws();
	test_unknown_location_directive_throws();
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
	test_single_server_name();
	test_several_names_in_one_directive();
	test_repeated_server_name_directives_accumulate();
	test_server_name_is_lowercased();
	test_no_server_name_leaves_list_empty();
	test_server_names_are_per_server();
	test_server_name_without_argument_throws();
	test_default_body_size();
	test_body_size_in_bytes();
	test_body_size_suffixes();
	test_location_body_size_overrides_server();
	test_location_without_body_size_inherits();
	test_malformed_body_size_throws();
	test_location_without_cgi_pass_is_empty();
	test_cgi_pass_binds_python();
	test_cgi_pass_accumulates_extensions();
	test_cgi_pass_binds_php();
	test_last_cgi_pass_wins();
	test_cgi_pass_is_per_location();
	test_malformed_cgi_pass_throws();
	test_location_without_return_is_zero();
	test_return_codes_are_parsed();
	test_return_accepts_absolute_url();
	test_return_is_per_location();
	test_last_return_wins();
	test_malformed_return_throws();
	test_static_helpers();

	std::cout << std::endl;
	std::cout << s_pass << " passed, " << s_fail << " failed, "
	          << (s_pass + s_fail) << " total" << std::endl;

	return (s_fail == 0 ? 0 : 1);
}

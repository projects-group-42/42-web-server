/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   server_config_test.cpp                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dajesus- <dajesus-@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/20 22:07:22 by dajesus-          #+#    #+#             */
/*   Updated: 2026/07/20 23:22:33 by dajesus-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

# include <iostream>
# include <string>

# include "config/ServerConfig.hpp"

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
/* ServerConfig / LocationConfig tests                                 */
/* ------------------------------------------------------------------ */

static void	test_server_config_defaults(void)
{
	ServerConfig	config;

	CHECK_EQ(config.host, std::string("0.0.0.0"), "default host is 0.0.0.0");
	CHECK_EQ(config.port, 8080, "default port is 8080");
	CHECK_EQ(config.serverNames.size(), static_cast<size_t>(0),
	         "default server has no server_name entries");
	CHECK_EQ(config.index, std::string("index.html"),
	         "default index is index.html");
	CHECK_EQ(config.errorPages.size(), static_cast<size_t>(0),
	         "default server has no custom error pages");
	CHECK_EQ(config.locations.size(), static_cast<size_t>(0),
	         "default server has no location blocks");
	TEST(config.clientMaxBodySize > 0,
	     "default client_max_body_size is a positive default");
}

static void	test_server_config_is_mutable_and_holds_locations(void)
{
	ServerConfig	config;

	config.host = "127.0.0.1";
	config.port = 8081;
	config.serverNames.push_back("example.com");
	config.root = "/var/www";
	config.errorPages[404] = "/404.html";

	LocationConfig	root("/");

	root.allowedMethods.push_back("GET");
	root.allowedMethods.push_back("POST");
	root.autoindex = false;
	config.locations.push_back(root);

	LocationConfig	uploads("/uploads");

	uploads.uploadStore = "/var/uploads";
	uploads.allowedMethods.push_back("POST");
	uploads.allowedMethods.push_back("DELETE");
	config.locations.push_back(uploads);

	CHECK_EQ(config.host, std::string("127.0.0.1"), "host can be overridden");
	CHECK_EQ(config.errorPages[404], std::string("/404.html"),
	         "error_page 404 stored under its status code");
	CHECK_EQ(config.locations.size(), static_cast<size_t>(2),
	         "server holds every configured location");
	CHECK_EQ(config.locations[0].path, std::string("/"),
	         "first location path is preserved");
	CHECK_EQ(config.locations[1].uploadStore, std::string("/var/uploads"),
	         "second location keeps its upload_store");
}

static void	test_location_config_defaults(void)
{
	LocationConfig	location;

	CHECK_EQ(location.path, std::string(""), "default location path is empty");
	CHECK_EQ(location.autoindex, false, "default autoindex is off");
	CHECK_EQ(location.allowedMethods.size(), static_cast<size_t>(0),
	         "default location has no method restriction");
	CHECK_EQ(location.returnCode, 0,
	         "default location has no return configured");
	CHECK_EQ(location.clientMaxBodySize, -1L,
	         "default location inherits client_max_body_size from server");
}

int	main(void)
{
	test_server_config_defaults();
	test_server_config_is_mutable_and_holds_locations();
	test_location_config_defaults();

	std::cout << std::endl;
	std::cout << s_pass << " passed, " << s_fail << " failed, "
	          << (s_pass + s_fail) << " total" << std::endl;

	return (s_fail == 0 ? 0 : 1);
}

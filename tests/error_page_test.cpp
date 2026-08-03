/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   error_page_test.cpp                                :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: galves-a <galves-a@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/08/02 00:00:00 by jucoelho          #+#    #+#             */
/*   Updated: 2026/08/03 03:40:00 by galves-a         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

# include <iostream>
# include <string>
# include <vector>
# include <fstream>
# include <cstdio>
# include <stdexcept>
# include <sys/stat.h>
# include <unistd.h>

# include "config/ConfigLoader.hpp"
# include "config/ServerConfig.hpp"
# include "http/Router.hpp"
# include "http/HttpRequest.hpp"
# include "http/HttpResponse.hpp"

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

static bool createDirectory(const std::string &path)
{
	int result = mkdir(path.c_str(), 0755);
	return (result == 0 || errno == EEXIST);
}

static bool writeFile(const std::string &path, const std::string &content)
{
	std::ofstream file(path.c_str());
	if (!file.is_open())
		return (false);
	file << content;
	return (file.good());
}

static void cleanupFile(const std::string &path)
{
	remove(path.c_str());
}

static void cleanupDirectory(const std::string &path)
{
	rmdir(path.c_str());
}

static void test_config_loader_parses_error_page(void)
{
	const std::string configPath = "tests/tmp_error_page.conf";
	std::ofstream configFile(configPath.c_str());
	configFile << "server {\n";
	configFile << "    listen 8080;\n";
	configFile << "    root tests/tmp_root;\n";
	configFile << "    error_page 404 /errors/404.html;\n";
	configFile << "    error_page 500 502 503 504 /errors/500.html;\n";
	configFile << "}\n";
	configFile.close();

	ConfigLoader loader(configPath);
	std::vector<ServerConfig> servers = loader.loader();

	TEST(servers.size() == 1, "ConfigLoader loads one server block");
	if (servers.size() == 1)
	{
		CHECK_EQ(servers[0].errorPages.size(), static_cast<size_t>(5),
		         "ConfigLoader stores all error_page status codes");
		CHECK_EQ(servers[0].errorPages[404], std::string("/errors/404.html"),
		         "ConfigLoader stores 404 error_page path");
		CHECK_EQ(servers[0].errorPages[500], std::string("/errors/500.html"),
		         "ConfigLoader stores 500 error_page path");
		CHECK_EQ(servers[0].errorPages[502], std::string("/errors/500.html"),
		         "ConfigLoader stores 502 error_page path");
		CHECK_EQ(servers[0].errorPages[503], std::string("/errors/500.html"),
		         "ConfigLoader stores 503 error_page path");
		CHECK_EQ(servers[0].errorPages[504], std::string("/errors/500.html"),
		         "ConfigLoader stores 504 error_page path");
	}

	cleanupFile(configPath);
}

static void test_router_uses_configured_error_page(void)
{
	const std::string rootPath = "tests/tmp_root";
	const std::string errorsPath = rootPath + "/errors";
	const std::string errorFile = errorsPath + "/404.html";
	const std::string errorBody = "<html><body><h1>Custom 404</h1></body></html>";

	createDirectory(rootPath);
	createDirectory(errorsPath);
	writeFile(errorFile, errorBody);

	ServerConfig config;
	config.root = rootPath;
	config.index = "index.html";
	config.errorPages[404] = "/errors/404.html";

	HttpRequest request;
	request.setMethod("GET");
	request.setUri("/missing-file");
	request.setVersion("HTTP/1.1");
	request.setHeaders("Host", "localhost");

	HttpResponse response;
	Router router;
	router.route(request, response, config);

	CHECK_EQ(response.getStatusCode(), 404,
	         "Router returns 404 for missing resource");
	CHECK_EQ(response.getBody(), errorBody,
	         "Router serves configured error page body");
	CHECK_EQ(response.getHeaderValue("content-type"), std::string("text/html"),
	         "Router sets content-type for custom HTML error page");

	cleanupFile(errorFile);
	cleanupDirectory(errorsPath);
	cleanupDirectory(rootPath);
}

static void test_error_page_pointing_at_directory(void)
{
	const std::string rootPath = "tests/tmp_root";
	const std::string dirPath = rootPath + "/adir";

	createDirectory(rootPath);
	createDirectory(dirPath);

	ServerConfig config;
	config.root = rootPath;
	config.index = "index.html";
	config.errorPages[404] = "/adir";

	std::string body;
	std::string contentType;
	bool loaded = Router::loadErrorPage(config, config.root, 404, body,
			contentType);

	TEST(!loaded, "loadErrorPage refuses a directory instead of throwing");
	TEST(body.empty(), "loadErrorPage leaves the body empty for a directory");

	HttpRequest request;
	request.setMethod("GET");
	request.setUri("/missing-file");
	request.setVersion("HTTP/1.1");
	request.setHeaders("Host", "localhost");

	HttpResponse response;
	Router router;
	router.route(request, response, config);

	CHECK_EQ(response.getStatusCode(), 404,
	         "Router keeps the 404 status when the error page is a directory");

	cleanupDirectory(dirPath);
	cleanupDirectory(rootPath);
}

static void test_error_page_missing_file_keeps_status(void)
{
	ServerConfig config;
	config.root = "tests/tmp_root";
	config.errorPages[404] = "/errors/does-not-exist.html";

	std::string body;
	std::string contentType;

	TEST(!Router::loadErrorPage(config, config.root, 404, body, contentType),
	     "loadErrorPage returns false for a missing file");
	TEST(!Router::loadErrorPage(config, config.root, 500, body, contentType),
	     "loadErrorPage returns false for an unconfigured status");
}

static void test_config_loader_rejects_invalid_error_page(void)
{
	const std::string configPath = "tests/tmp_invalid_error_page.conf";
	bool threw;

	std::ofstream badCode(configPath.c_str());
	badCode << "server {\n    listen 8080;\n    error_page abc /x.html;\n}\n";
	badCode.close();
	threw = false;
	try
	{
		ConfigLoader loader(configPath);
		loader.loader();
	}
	catch (const std::exception &)
	{
		threw = true;
	}
	TEST(threw, "ConfigLoader rejects a non-numeric error_page status");

	std::ofstream outOfRange(configPath.c_str());
	outOfRange << "server {\n    listen 8080;\n    error_page 200 /x.html;\n}\n";
	outOfRange.close();
	threw = false;
	try
	{
		ConfigLoader loader(configPath);
		loader.loader();
	}
	catch (const std::exception &)
	{
		threw = true;
	}
	TEST(threw, "ConfigLoader rejects an error_page status outside 400-599");

	std::ofstream missingPath(configPath.c_str());
	missingPath << "server {\n    listen 8080;\n    error_page 404;\n}\n";
	missingPath.close();
	threw = false;
	try
	{
		ConfigLoader loader(configPath);
		loader.loader();
	}
	catch (const std::exception &)
	{
		threw = true;
	}
	TEST(threw, "ConfigLoader rejects error_page without a path");

	cleanupFile(configPath);
}

static void test_config_loader_rejects_empty_location_directive(void)
{
	const std::string configPath = "tests/tmp_empty_location.conf";
	bool threw;

	std::ofstream rootFile(configPath.c_str());
	rootFile << "server {\n    listen 8080;\n    location / {\n"
		<< "        root;\n    }\n}\n";
	rootFile.close();
	threw = false;
	try
	{
		ConfigLoader loader(configPath);
		loader.loader();
	}
	catch (const std::exception &)
	{
		threw = true;
	}
	TEST(threw, "ConfigLoader rejects an argument-less root inside location");

	std::ofstream indexFile(configPath.c_str());
	indexFile << "server {\n    listen 8080;\n    location / {\n"
		<< "        index;\n    }\n}\n";
	indexFile.close();
	threw = false;
	try
	{
		ConfigLoader loader(configPath);
		loader.loader();
	}
	catch (const std::exception &)
	{
		threw = true;
	}
	TEST(threw, "ConfigLoader rejects an argument-less index inside location");

	cleanupFile(configPath);
}

int	main(void)
{
	test_config_loader_parses_error_page();
	test_router_uses_configured_error_page();
	test_error_page_pointing_at_directory();
	test_error_page_missing_file_keeps_status();
	test_config_loader_rejects_invalid_error_page();
	test_config_loader_rejects_empty_location_directive();

	std::cout << std::endl;
	std::cout << s_pass << " passed, " << s_fail << " failed, "
		  << (s_pass + s_fail) << " total" << std::endl;

	return (s_fail == 0 ? 0 : 1);
}

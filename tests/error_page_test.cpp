/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   error_page_test.cpp                                :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: galves-a <galves-a@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/08/02 00:00:00 by jucoelho          #+#    #+#             */
/*   Updated: 2026/08/03 22:41:07 by galves-a         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

# include <cerrno>
# include <cstdio>
# include <fstream>
# include <iostream>
# include <stdexcept>
# include <string>
# include <sys/stat.h>
# include <unistd.h>
# include <vector>

# include "config/ConfigLoader.hpp"
# include "config/ServerConfig.hpp"
# include "http/HttpRequest.hpp"
# include "http/HttpResponse.hpp"
# include "http/Router.hpp"

static int	s_pass = 0;
static int	s_fail = 0;

# define TEST(cond, name) \
	do { \
		if (cond) { \
			s_pass++; \
			std::cout << "[PASS] " << name << std::endl; \
		} else { \
			s_fail++; \
			std::cerr << "[FAIL] " << name << std::endl; \
		} \
	} while (0)

# define CHECK_EQ(a, b, msg) TEST((a) == (b), msg)

/**
 * @brief Creates a directory, tolerating one that already exists.
 * @param path The directory to create.
 * @return true when the directory exists once the call returns.
 */
static bool	createDirectory(const std::string &path)
{
	return (mkdir(path.c_str(), 0755) == 0 || errno == EEXIST);
}

/**
 * @brief Writes a file with the given contents.
 * @param path The file to write.
 * @param content The contents to write into it.
 * @return true when the whole content reached the disk.
 */
static bool	writeFile(const std::string &path, const std::string &content)
{
	std::ofstream	file(path.c_str());

	if (!file.is_open())
		return (false);
	file << content;
	return (file.good());
}

/**
 * @brief Writes a config file and loads it, reporting whether it was refused.
 * @param body The config file contents to load.
 * @return true when ConfigLoader threw on the given config.
 */
static bool	loadRejects(const std::string &body)
{
	const std::string	path = "tests/tmp_error_page_invalid.conf";
	bool				threw = false;

	writeFile(path, body);
	try
	{
		ConfigLoader	loader(path);
		loader.loader();
	}
	catch (const std::exception &)
	{
		threw = true;
	}
	remove(path.c_str());
	return (threw);
}

/**
 * @brief Checks that error_page directives land in the map of the server.
 */
static void	test_loader_parses_error_page(void)
{
	const std::string	path = "tests/tmp_error_page.conf";

	writeFile(path,
		"server {\n"
		"    listen 8080;\n"
		"    root tests/tmp_root;\n"
		"    error_page 404 /errors/404.html;\n"
		"    error_page 500 502 503 504 /errors/500.html;\n"
		"}\n");

	ConfigLoader				loader(path);
	std::vector<ServerConfig>	servers = loader.loader();

	CHECK_EQ(servers.size(), static_cast<size_t>(1),
		"error_page config loads a single server block");
	if (servers.size() == 1)
	{
		CHECK_EQ(servers[0].errorPages.size(), static_cast<size_t>(5),
			"every status code of both directives is stored");
		CHECK_EQ(servers[0].errorPages[404], std::string("/errors/404.html"),
			"404 maps to its own page");
		CHECK_EQ(servers[0].errorPages[500], std::string("/errors/500.html"),
			"500 maps to the shared page");
		CHECK_EQ(servers[0].errorPages[504], std::string("/errors/500.html"),
			"504 maps to the shared page");
	}
	remove(path.c_str());
}

/**
 * @brief Checks that a status code declared twice keeps the last page.
 */
static void	test_loader_last_declaration_wins(void)
{
	const std::string	path = "tests/tmp_error_page_dup.conf";

	writeFile(path,
		"server {\n"
		"    listen 8080;\n"
		"    error_page 404 /first.html;\n"
		"    error_page 404 /second.html;\n"
		"}\n");

	ConfigLoader				loader(path);
	std::vector<ServerConfig>	servers = loader.loader();

	CHECK_EQ(servers[0].errorPages.size(), static_cast<size_t>(1),
		"a status declared twice is stored once");
	CHECK_EQ(servers[0].errorPages[404], std::string("/second.html"),
		"the last error_page declaration wins");
	remove(path.c_str());
}

/**
 * @brief Checks that malformed error_page directives are refused at load time.
 */
static void	test_loader_rejects_invalid_error_page(void)
{
	TEST(loadRejects("server {\n    listen 8080;\n"
			"    error_page abc /x.html;\n}\n"),
		"a non-numeric status code is refused");
	TEST(loadRejects("server {\n    listen 8080;\n"
			"    error_page 200 /x.html;\n}\n"),
		"a status code below 400 is refused");
	TEST(loadRejects("server {\n    listen 8080;\n"
			"    error_page 600 /x.html;\n}\n"),
		"a status code above 599 is refused");
	TEST(loadRejects("server {\n    listen 8080;\n"
			"    error_page 4041 /x.html;\n}\n"),
		"a status code longer than three digits is refused");
	TEST(loadRejects("server {\n    listen 8080;\n"
			"    error_page 404;\n}\n"),
		"an error_page without a path is refused");
	TEST(loadRejects("server {\n    listen 8080;\n"
			"    error_page 404 500 /a.html;\n    error_page 40x /b.html;\n}\n"),
		"one bad code refuses the whole config");
}

/**
 * @brief Checks that a 404 answer carries the body of the configured page.
 */
static void	test_router_serves_configured_page(void)
{
	const std::string	root = "tests/tmp_root";
	const std::string	dir = root + "/errors";
	const std::string	file = dir + "/404.html";
	const std::string	page = "<html><body><h1>Custom 404</h1></body></html>";

	createDirectory(root);
	createDirectory(dir);
	writeFile(file, page);

	ServerConfig	config;
	config.root = root;
	config.index = "index.html";
	config.errorPages[404] = "/errors/404.html";

	HttpRequest	request;
	request.setMethod("GET");
	request.setUri("/missing-file");
	request.setVersion("HTTP/1.1");
	request.setHeaders("Host", "localhost");

	HttpResponse	response;
	Router			router;

	router.route(request, response, config);

	CHECK_EQ(response.getStatusCode(), 404,
		"a missing file still answers 404");
	CHECK_EQ(response.getBody(), page,
		"the configured page becomes the response body");
	CHECK_EQ(response.getHeaderValue("content-type"), std::string("text/html"),
		"the content-type matches the configured page");

	remove(file.c_str());
	rmdir(dir.c_str());
	rmdir(root.c_str());
}

/**
 * @brief Checks that a location root overrides the server root for the page.
 */
static void	test_router_resolves_page_against_location_root(void)
{
	const std::string	root = "tests/tmp_loc_root";
	const std::string	dir = root + "/errors";
	const std::string	file = dir + "/404.html";
	const std::string	page = "<html><body><h1>Location 404</h1></body></html>";

	createDirectory(root);
	createDirectory(dir);
	writeFile(file, page);

	ServerConfig	config;
	config.root = "tests/does-not-exist";
	config.errorPages[404] = "/errors/404.html";

	LocationConfig	location("/scoped");
	location.root = root;
	config.locations.push_back(location);

	HttpRequest	request;
	request.setMethod("GET");
	request.setUri("/scoped/missing-file");
	request.setVersion("HTTP/1.1");
	request.setHeaders("Host", "localhost");

	HttpResponse	response;
	Router			router;

	router.route(request, response, config);

	CHECK_EQ(response.getStatusCode(), 404,
		"a missing file under a location answers 404");
	CHECK_EQ(response.getBody(), page,
		"the page is resolved against the root of the location");

	remove(file.c_str());
	rmdir(dir.c_str());
	rmdir(root.c_str());
}

/**
 * @brief Checks that a trailing slash on the root does not double the separator.
 */
static void	test_router_normalises_root_separator(void)
{
	const std::string	root = "tests/tmp_slash_root";
	const std::string	file = root + "/404.html";
	const std::string	page = "slash";

	createDirectory(root);
	writeFile(file, page);

	ServerConfig	config;
	config.root = root + "/";
	config.errorPages[404] = "404.html";

	std::string	body;
	std::string	contentType;

	TEST(Router::loadErrorPage(config, config.root, 404, body, contentType),
		"a root ending in '/' and a relative page still resolve");
	CHECK_EQ(body, page, "the body of the page is read whole");

	remove(file.c_str());
	rmdir(root.c_str());
}

/**
 * @brief Checks that an unreadable page leaves the built-in body in place.
 */
static void	test_router_falls_back_on_unreadable_page(void)
{
	const std::string	root = "tests/tmp_root";
	const std::string	dir = root + "/adir";

	createDirectory(root);
	createDirectory(dir);

	ServerConfig	config;
	config.root = root;
	config.errorPages[404] = "/adir";
	config.errorPages[403] = "/errors/does-not-exist.html";

	std::string	body;
	std::string	contentType;

	TEST(!Router::loadErrorPage(config, config.root, 404, body, contentType),
		"a page pointing at a directory is refused");
	TEST(body.empty(), "a refused page leaves the body empty");
	TEST(!Router::loadErrorPage(config, config.root, 403, body, contentType),
		"a page pointing at a missing file is refused");
	TEST(!Router::loadErrorPage(config, config.root, 500, body, contentType),
		"a status without a configured page is refused");

	HttpRequest	request;
	request.setMethod("GET");
	request.setUri("/missing-file");
	request.setVersion("HTTP/1.1");
	request.setHeaders("Host", "localhost");

	HttpResponse	response;
	Router			router;

	router.route(request, response, config);
	CHECK_EQ(response.getStatusCode(), 404,
		"an unreadable page keeps the original status");

	rmdir(dir.c_str());
	rmdir(root.c_str());
}

/**
 * @brief Checks that a status raised without a handler still gets its page.
 * A 405 is answered before any handler runs, so it exercises the branch of
 * route() that sets an empty body, and its Allow header must survive.
 */
static void	test_router_serves_page_without_handler(void)
{
	const std::string	root = "tests/tmp_405_root";
	const std::string	file = root + "/405.html";
	const std::string	page = "<html><body><h1>Custom 405</h1></body></html>";

	createDirectory(root);
	writeFile(file, page);

	ServerConfig	config;
	config.root = root;
	config.errorPages[405] = "/405.html";

	HttpRequest	request;
	request.setMethod("PUT");
	request.setUri("/");
	request.setVersion("HTTP/1.1");
	request.setHeaders("Host", "localhost");

	HttpResponse	response;
	Router			router;

	router.route(request, response, config);

	CHECK_EQ(response.getStatusCode(), 405,
		"an unsupported method answers 405");
	CHECK_EQ(response.getBody(), page,
		"a status raised without a handler still gets its page");
	CHECK_EQ(response.getHeaderValue("Allow"),
		std::string("DELETE, GET, POST"),
		"replacing the body keeps the Allow header");

	remove(file.c_str());
	rmdir(root.c_str());
}

/**
 * @brief Checks that a successful answer is never rewritten by an error page.
 */
static void	test_router_leaves_success_untouched(void)
{
	const std::string	root = "tests/tmp_ok_root";
	const std::string	file = root + "/index.html";
	const std::string	page = "<html><body>ok</body></html>";

	createDirectory(root);
	writeFile(file, page);
	writeFile(root + "/404.html", "should not be served");

	ServerConfig	config;
	config.root = root;
	config.index = "index.html";
	config.errorPages[404] = "/404.html";

	HttpRequest	request;
	request.setMethod("GET");
	request.setUri("/index.html");
	request.setVersion("HTTP/1.1");
	request.setHeaders("Host", "localhost");

	HttpResponse	response;
	Router			router;

	router.route(request, response, config);

	CHECK_EQ(response.getStatusCode(), 200, "an existing file answers 200");
	CHECK_EQ(response.getBody(), page, "a 200 body is never replaced");

	remove(file.c_str());
	remove((root + "/404.html").c_str());
	rmdir(root.c_str());
}

int	main(void)
{
	test_loader_parses_error_page();
	test_loader_last_declaration_wins();
	test_loader_rejects_invalid_error_page();
	test_router_serves_configured_page();
	test_router_resolves_page_against_location_root();
	test_router_normalises_root_separator();
	test_router_falls_back_on_unreadable_page();
	test_router_serves_page_without_handler();
	test_router_leaves_success_untouched();

	std::cout << std::endl;
	std::cout << s_pass << " passed, " << s_fail << " failed, "
		<< (s_pass + s_fail) << " total" << std::endl;
	return (s_fail == 0 ? 0 : 1);
}

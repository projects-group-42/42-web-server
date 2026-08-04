/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   router_test.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: galves-a <galves-a@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/08/03 21:48:31 by galves-a          #+#    #+#             */
/*   Updated: 2026/08/03 21:48:31 by galves-a         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <iostream>
#include <fstream>
#include <string>
#include <cstdio>
#include <sys/stat.h>
#include <unistd.h>

#include "http/Router.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"
#include "config/ServerConfig.hpp"

static int	s_pass = 0;
static int	s_fail = 0;

# define TEST(cond, name) \
	do { \
		if (cond) { s_pass++; std::cout << "[PASS] " << name << std::endl; } \
		else { s_fail++; std::cerr << "[FAIL] " << name << std::endl; } \
	} while (0)

static const char	*ROOT_DIR = "tests/tmp_router";
static const char	*DOCS_DIR = "tests/tmp_router/docs";
static const char	*ALT_DIR = "tests/tmp_router/alt";
static const char	*ALT_DOCS_DIR = "tests/tmp_router/alt/docs";

/**
 * @brief Writes `content` into `path`, creating or truncating the file.
 * @param path The file to write.
 * @param content The bytes to store in it.
 */
static void	writeFile(const std::string &path, const std::string &content)
{
	std::ofstream	file(path.c_str());

	file << content;
	file.close();
}

/**
 * @brief Builds the fixture tree the routing tests serve their requests from.
 */
static void	setupFixture(void)
{
	mkdir(ROOT_DIR, 0755);
	mkdir(DOCS_DIR, 0755);
	mkdir(ALT_DIR, 0755);
	mkdir(ALT_DOCS_DIR, 0755);
	writeFile(std::string(ROOT_DIR) + "/home.html", "SERVER INDEX");
	writeFile(std::string(ROOT_DIR) + "/index.html", "DEFAULT INDEX");
	writeFile(std::string(DOCS_DIR) + "/manual.html", "LOCATION INDEX");
	writeFile(std::string(DOCS_DIR) + "/home.html", "INHERITED INDEX");
	writeFile(std::string(ALT_DOCS_DIR) + "/home.html", "LOCATION ROOT");
}

/**
 * @brief Removes the fixture tree created by setupFixture().
 */
static void	cleanupFixture(void)
{
	std::remove((std::string(ALT_DOCS_DIR) + "/home.html").c_str());
	std::remove((std::string(DOCS_DIR) + "/manual.html").c_str());
	std::remove((std::string(DOCS_DIR) + "/home.html").c_str());
	std::remove((std::string(ROOT_DIR) + "/home.html").c_str());
	std::remove((std::string(ROOT_DIR) + "/index.html").c_str());
	rmdir(ALT_DOCS_DIR);
	rmdir(ALT_DIR);
	rmdir(DOCS_DIR);
	rmdir(ROOT_DIR);
}

/**
 * @brief Routes a GET on `uri` through a fresh Router and returns the response.
 * @param uri The request target.
 * @param config The server block serving the request.
 * @param response The response filled by the router.
 */
static void	routeGet(const std::string &uri, const ServerConfig &config,
		HttpResponse &response)
{
	Router		router;
	HttpRequest	request;

	request.setMethod("GET");
	request.setUri(uri);
	request.setVersion("HTTP/1.1");
	router.route(request, response, config);
}

/**
 * @brief Builds a server block rooted at the fixture tree.
 * @param index The index directive value of the server block.
 * @return The populated server config.
 */
static ServerConfig	makeServer(const std::string &index)
{
	ServerConfig	config;

	config.root = ROOT_DIR;
	config.index = index;
	return (config);
}

int	main(void)
{
	setupFixture();

	{
		ServerConfig	config = makeServer("home.html");
		HttpResponse	response;

		routeGet("/", config, response);
		TEST(response.getStatusCode() == 200,
			"a server index other than index.html answers 200");
		TEST(response.getBody() == "SERVER INDEX",
			"a server index other than index.html is the file served");
	}

	{
		ServerConfig	config = makeServer("index.html");
		HttpResponse	response;

		routeGet("/", config, response);
		TEST(response.getBody() == "DEFAULT INDEX",
			"the default index is still served when configured");
	}

	{
		ServerConfig	config = makeServer("home.html");
		LocationConfig	docs("/docs");

		docs.index = "manual.html";
		config.locations.push_back(docs);

		HttpResponse	response;

		routeGet("/docs/", config, response);
		TEST(response.getBody() == "LOCATION INDEX",
			"a location index overrides the server index");
	}

	{
		ServerConfig	config = makeServer("home.html");
		LocationConfig	docs("/docs");

		config.locations.push_back(docs);

		HttpResponse	response;

		routeGet("/docs/", config, response);
		TEST(response.getBody() == "INHERITED INDEX",
			"a location without index falls back to the server index");
	}

	{
		ServerConfig	config = makeServer("home.html");
		LocationConfig	docs("/docs");

		docs.root = ALT_DIR;
		config.locations.push_back(docs);

		HttpResponse	response;

		routeGet("/docs/", config, response);
		TEST(response.getBody() == "LOCATION ROOT",
			"a location root overrides the server root");
	}

	{
		ServerConfig	custom = makeServer("home.html");
		ServerConfig	standard = makeServer("index.html");
		Router			router;
		HttpRequest		request;
		HttpResponse	first;
		HttpResponse	second;

		request.setMethod("GET");
		request.setUri("/");
		request.setVersion("HTTP/1.1");
		router.route(request, first, custom);
		router.route(request, second, standard);
		TEST(second.getBody() == "DEFAULT INDEX",
			"the index of one request does not leak into the next");
	}

	{
		ServerConfig	config = makeServer("missing.html");
		HttpResponse	response;

		routeGet("/", config, response);
		TEST(response.getStatusCode() == 404,
			"a directory without its configured index answers 404");
	}

	cleanupFixture();

	std::cout << std::endl << s_pass << " passed, " << s_fail << " failed"
		<< std::endl;
	return (s_fail == 0 ? 0 : 1);
}

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
	std::remove((std::string(ROOT_DIR) + "/upload.txt").c_str());
	std::remove((std::string(DOCS_DIR) + "/upload.txt").c_str());
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
 * @brief Routes a POST carrying `body` through a fresh Router.
 * @param uri The request target.
 * @param body The request body to send.
 * @param config The server block serving the request.
 * @param response The response filled by the router.
 */
static void	routePost(const std::string &uri, const std::string &body,
		const ServerConfig &config, HttpResponse &response)
{
	Router		router;
	HttpRequest	request;

	request.setMethod("POST");
	request.setUri(uri);
	request.setVersion("HTTP/1.1");
	request.setBody(body);
	router.route(request, response, config);
}

/**
 * @brief Builds a POST request carrying a body for a URI.
 * @param uri The request target.
 * @param body The bytes the request carries.
 * @return The populated request.
 */
static HttpRequest	makePost(const std::string &uri, const std::string &body)
{
	HttpRequest	request;

	request.setMethod("POST");
	request.setUri(uri);
	request.setVersion("HTTP/1.1");
	request.setBody(body);
	return (request);
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

	{
		ServerConfig	config = makeServer("index.html");
		HttpResponse	response;

		config.clientMaxBodySize = 4;
		routePost("/upload.txt", "way over the limit", config, response);
		TEST(response.getStatusCode() == 413,
			"a body over the server client_max_body_size answers 413");
	}

	{
		ServerConfig	config = makeServer("index.html");
		HttpResponse	response;

		config.clientMaxBodySize = 1024;
		routePost("/upload.txt", "small", config, response);
		TEST(response.getStatusCode() != 413,
			"a body under the server client_max_body_size is accepted");
	}

	{
		ServerConfig	config = makeServer("index.html");
		LocationConfig	upload("/docs");
		HttpResponse	response;

		config.clientMaxBodySize = 1024;
		upload.clientMaxBodySize = 4;
		config.locations.push_back(upload);
		routePost("/docs/upload.txt", "way over the limit", config, response);
		TEST(response.getStatusCode() == 413,
			"a location client_max_body_size overrides the server value");
	}

	{
		ServerConfig	config = makeServer("index.html");
		LocationConfig	upload("/docs");
		HttpResponse	response;

		config.clientMaxBodySize = 4;
		config.locations.push_back(upload);
		routePost("/docs/upload.txt", "way over the limit", config, response);
		TEST(response.getStatusCode() == 413,
			"a location without client_max_body_size inherits the server value");
	}

	{
		ServerConfig	tight = makeServer("index.html");
		ServerConfig	loose = makeServer("index.html");
		Router			router;
		HttpRequest		request;
		HttpResponse	first;
		HttpResponse	second;

		tight.clientMaxBodySize = 4;
		loose.clientMaxBodySize = 1024;
		request.setMethod("POST");
		request.setUri("/upload.txt");
		request.setVersion("HTTP/1.1");
		request.setBody("way over the limit");
		router.route(request, first, tight);
		router.route(request, second, loose);
		TEST(second.getStatusCode() != 413,
			"the body limit of one request does not leak into the next");
	}

	{
		ServerConfig	config = makeServer("index.html");
		LocationConfig	cgi("/cgi");
		Router			router;

		config.clientMaxBodySize = 1024;
		cgi.clientMaxBodySize = 4;
		cgi.cgiPass[".py"] = "/usr/bin/python3";
		config.locations.push_back(cgi);
		TEST(router.bodyExceedsLimit(makePost("/cgi/app.py", "over"), config)
				== false,
			"a CGI body at the location client_max_body_size is accepted");
		TEST(router.bodyExceedsLimit(
				makePost("/cgi/app.py", "way over the limit"), config),
			"a CGI body over the location client_max_body_size is refused");
		TEST(router.bodyExceedsLimit(makePost("/cgi/app.py", "ok"), config)
				== false,
			"a CGI body under the location client_max_body_size is accepted");
	}

	{
		ServerConfig	config = makeServer("index.html");
		LocationConfig	cgi("/cgi");
		Router			router;

		config.clientMaxBodySize = 4;
		cgi.cgiPass[".py"] = "/usr/bin/python3";
		config.locations.push_back(cgi);
		TEST(router.bodyExceedsLimit(
				makePost("/cgi/app.py", "way over the limit"), config),
			"a CGI location without a limit inherits the server value");
	}

	{
		ServerConfig	config = makeServer("index.html");
		Router			router;

		config.clientMaxBodySize = -1;
		TEST(router.bodyExceedsLimit(
				makePost("/upload.txt", "way over the limit"), config) == false,
			"a negative client_max_body_size lifts the limit");
	}

	{
		ServerConfig	config = makeServer("index.html");
		LocationConfig	cgi("/cgi");
		Router			router;

		cgi.cgiPass[".py"] = "/usr/bin/python3";
		config.locations.push_back(cgi);
		TEST(router.resolveCgiInterpreter("/cgi/app.py", config)
				== "/usr/bin/python3",
			"the interpreter bound to .py runs a python script");
		TEST(router.resolveCgiInterpreter("/cgi/app.pl", config).empty(),
			"an extension no cgi_pass binds resolves to no interpreter");
		TEST(router.resolveCgiInterpreter("/cgi/app", config).empty(),
			"a script without an extension resolves to no interpreter");
		TEST(router.resolveCgiInterpreter("/other/app.py", config).empty(),
			"a URI outside the location resolves to no interpreter");
	}

	{
		ServerConfig	config = makeServer("index.html");
		LocationConfig	root("/");
		LocationConfig	cgi("/cgi");
		Router			router;

		root.cgiPass[".py"] = "/usr/bin/python2";
		cgi.cgiPass[".py"] = "/usr/bin/python3";
		config.locations.push_back(root);
		config.locations.push_back(cgi);
		TEST(router.resolveCgiInterpreter("/cgi/app.py", config)
				== "/usr/bin/python3",
			"the longest matching location provides the interpreter");
		TEST(router.resolveCgiInterpreter("/app.py", config)
				== "/usr/bin/python2",
			"a URI outside it falls back to the shorter location");
	}

	{
		ServerConfig	config = makeServer("index.html");
		LocationConfig	cgi("/cgi");
		Router			router;

		config.locations.push_back(cgi);
		TEST(router.resolveCgiInterpreter("/cgi/app.py", config).empty(),
			"a location without cgi_pass resolves to no interpreter");
		TEST(router.resolveCgiInterpreter("/cgi.d/app", config).empty(),
			"a dot in a parent directory is not taken as an extension");
	}

	{
		ServerConfig	config = makeServer("index.html");
		LocationConfig	cgi("/cgi");
		Router			router;

		cgi.cgiPass[".py"] = "/usr/bin/python3";
		cgi.cgiPass[".php"] = "/usr/bin/php-cgi";
		config.locations.push_back(cgi);
		TEST(router.resolveCgiInterpreter("/cgi/app.php", config)
				== "/usr/bin/php-cgi",
			"the interpreter bound to .php runs a php script");
		TEST(router.resolveCgiInterpreter("/cgi/app.py", config)
				== "/usr/bin/python3",
			"binding .php leaves the .py binding untouched");
		TEST(router.resolveCgiInterpreter("/cgi-bin/hello.php", config)
				== "/usr/bin/php-cgi",
			"a /cgi-bin script resolves through the location prefixing it");
		TEST(router.resolveCgiInterpreter("/cgi/app.phps", config).empty(),
			"an extension merely starting with .php is not bound");
	}

	{
		ServerConfig	config = makeServer("index.html");
		LocationConfig	old("/old");
		HttpResponse	response;

		old.returnCode = 301;
		old.returnUrl = "/new";
		config.locations.push_back(old);
		routeGet("/old", config, response);
		TEST(response.getStatusCode() == 301,
			"a location declaring return answers its status");
		TEST(response.getHeaderValue("Location") == "/new",
			"a redirect carries the configured Location header");
		TEST(response.getBody().empty(),
			"a redirect answers no body of its own");
	}

	{
		ServerConfig	config = makeServer("index.html");
		LocationConfig	old("/old");
		HttpResponse	found;
		HttpResponse	deep;

		old.returnCode = 302;
		old.returnUrl = "/new";
		config.locations.push_back(old);
		routeGet("/old", config, found);
		TEST(found.getStatusCode() == 302,
			"return 302 answers 302");
		routeGet("/old/deeper/page.html", config, deep);
		TEST(deep.getStatusCode() == 302,
			"every URI under the location is redirected");
	}

	{
		ServerConfig	config = makeServer("index.html");
		LocationConfig	old("/old");
		HttpResponse	response;

		old.returnCode = 301;
		old.returnUrl = "/new";
		config.locations.push_back(old);
		routePost("/old", "body", config, response);
		TEST(response.getStatusCode() == 301,
			"a redirect applies to every method, not only GET");
	}

	{
		ServerConfig	config = makeServer("index.html");
		HttpResponse	response;

		routeGet("/", config, response);
		TEST(response.getStatusCode() == 200,
			"a server without any return is not redirected");
		TEST(response.getHeaderValue("Location").empty(),
			"a plain response carries no Location header");
	}

	{
		ServerConfig	config = makeServer("index.html");
		LocationConfig	cgi("/cgi");
		Router			router;

		cgi.cgiPass[".py"] = "/usr/bin/python3";
		cgi.returnCode = 301;
		cgi.returnUrl = "/new";
		config.locations.push_back(cgi);
		TEST(router.redirects("/cgi/app.py", config),
			"a location declaring return reports a redirect");
		TEST(!router.redirects("/other/app.py", config),
			"a URI outside the location reports no redirect");
	}

	cleanupFixture();

	std::cout << std::endl << s_pass << " passed, " << s_fail << " failed"
		<< std::endl;
	return (s_fail == 0 ? 0 : 1);
}

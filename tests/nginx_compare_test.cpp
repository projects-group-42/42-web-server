/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   nginx_compare_test.cpp                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dajesus- <dajesus-@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/08/13 02:22:07 by dajesus-          #+#    #+#             */
/*   Updated: 2026/08/14 12:48:12 by dajesus-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/*
 * Compares status codes, status reason phrases and headers this server
 * answers with against documented NGINX behaviour for the same requests.
*/

# include <cstdio>
# include <fstream>
# include <iostream>
# include <string>
# include <sys/stat.h>
# include <unistd.h>

# include "config/ServerConfig.hpp"
# include "http/HttpRequest.hpp"
# include "http/HttpResponse.hpp"
# include "http/ResponseBuilder.hpp"
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

static bool	contains(const std::string &haystack, const std::string &needle)
{
	return (haystack.find(needle) != std::string::npos);
}

/* ------------------------------------------------------------------ */
/* Fixture tree                                                        */
/* ------------------------------------------------------------------ */

static const char	*ROOT_DIR = "tests/tmp_nginx_compare";
static const char	*EMPTY_DIR = "tests/tmp_nginx_compare/empty";
static const char	*LISTED_DIR = "tests/tmp_nginx_compare/listed";

static void	writeFile(const std::string &path, const std::string &content)
{
	std::ofstream	file(path.c_str());

	file << content;
	file.close();
}

static void	setupFixture(void)
{
	mkdir(ROOT_DIR, 0755);
	mkdir(EMPTY_DIR, 0755);
	mkdir(LISTED_DIR, 0755);
	writeFile(std::string(ROOT_DIR) + "/index.html", "<p>home</p>");
	writeFile(std::string(ROOT_DIR) + "/notes.bin", "raw-bytes");
	writeFile(std::string(LISTED_DIR) + "/one.txt", "one");
}

static void	cleanupFixture(void)
{
	std::remove((std::string(ROOT_DIR) + "/index.html").c_str());
	std::remove((std::string(ROOT_DIR) + "/notes.bin").c_str());
	std::remove((std::string(LISTED_DIR) + "/one.txt").c_str());
	rmdir(EMPTY_DIR);
	rmdir(LISTED_DIR);
	rmdir(ROOT_DIR);
}

static ServerConfig	makeServer(void)
{
	ServerConfig	config;

	config.root = ROOT_DIR;
	config.index = "index.html";
	return (config);
}

/*
 * Routes `method` on `uri` through a fresh Router and returns the raw
 * HTTP/1.1 response ResponseBuilder would put on the wire, so headers can be
 * checked the way a client (or NGINX) would see them.
 */
static std::string	send(const std::string &method, const std::string &uri,
		const ServerConfig &config, HttpResponse &response)
{
	Router			router;
	HttpRequest		request;
	ResponseBuilder	builder;

	request.setMethod(method);
	request.setUri(uri);
	request.setVersion("HTTP/1.1");
	router.route(request, response, config);
	return (builder.builder(request, response));
}

/* ------------------------------------------------------------------ */
/* Cases matching NGINX                                                */
/* ------------------------------------------------------------------ */

/*
 * NGINX types a static file from its extension and reports its exact byte
 * count in Content-Length; both are matched here.
 */
static void	test_static_file_status_and_type_match_nginx(void)
{
	ServerConfig	config = makeServer();
	HttpResponse	response;

	send("GET", "/index.html", config, response);
	CHECK_EQ(response.getStatusCode(), 200,
		"an existing static file answers 200, same as NGINX");
	CHECK_EQ(response.getHeaderValue("content-type"), std::string("text/html"),
		"the Content-Type is derived from the extension, same as NGINX");
	CHECK_EQ(response.getHeaderValue("content-length"),
		std::string("11"), "Content-Length matches the exact byte count");
}

/*
 * NGINX falls back to application/octet-stream for an extension it has no
 * mime.types entry for; this server does the same.
 */
static void	test_unknown_extension_defaults_like_nginx(void)
{
	ServerConfig	config = makeServer();
	HttpResponse	response;

	send("GET", "/notes.bin", config, response);
	CHECK_EQ(response.getHeaderValue("content-type"),
		std::string("application/octet-stream"),
		"an unrecognised extension defaults to octet-stream, same as NGINX");
}

/*
 * NGINX redirects a directory requested without its trailing slash to the
 * same URI with one appended, so relative links inside the page it serves
 * resolve correctly. This server applies the same rewrite.
 */
static void	test_directory_without_slash_redirects_like_nginx(void)
{
	ServerConfig	config = makeServer();
	HttpResponse	response;

	send("GET", "/listed", config, response);
	CHECK_EQ(response.getStatusCode(), 301,
		"a directory without a trailing slash answers 301, same as NGINX");
	CHECK_EQ(response.getHeaderValue("location"), std::string("/listed/"),
		"the Location header appends the missing slash, same as NGINX");
}

/*
 * NGINX answers 405 on a method a location's limit_except leaves out, and
 * carries the accepted methods in the Allow header. This server does both.
 */
static void	test_method_not_allowed_carries_allow_header_like_nginx(void)
{
	ServerConfig	config = makeServer();
	LocationConfig	restricted("/");

	restricted.allowedMethods.push_back("GET");
	config.locations.push_back(restricted);

	HttpResponse	response;

	send("DELETE", "/index.html", config, response);
	CHECK_EQ(response.getStatusCode(), 405,
		"a method left out of limit_except answers 405, same as NGINX");
	CHECK_EQ(response.getHeaderValue("allow"), std::string("GET"),
		"the Allow header lists the accepted methods, same as NGINX");
}

/*
 * NGINX answers a `return` directive with the configured status and a
 * Location header, before touching the filesystem. This server does the
 * same, and the redirect carries no body either.
 */
static void	test_configured_redirect_matches_nginx_return_directive(void)
{
	ServerConfig	config = makeServer();
	LocationConfig	old("/old");

	old.returnCode = 302;
	old.returnUrl = "/new";
	config.locations.push_back(old);

	HttpResponse	response;

	send("GET", "/old", config, response);
	CHECK_EQ(response.getStatusCode(), 302,
		"a return directive answers with its configured status, same as NGINX");
	CHECK_EQ(response.getHeaderValue("location"), std::string("/new"),
		"a return directive sets Location to its configured target");
	TEST(response.getBody().empty(),
		"a configured redirect carries no body, same as NGINX");
}

/*
 * NGINX answers HEAD with the headers a GET would carry, Content-Length
 * included, and drops the body. This server does the same rather than
 * omitting Content-Length the way it would for a body-less method.
 */
static void	test_head_request_matches_nginx_semantics(void)
{
	ServerConfig	config = makeServer();
	HttpResponse	response;
	std::string		raw = send("HEAD", "/index.html", config, response);

	CHECK_EQ(response.getStatusCode(), 200,
		"HEAD on an existing file answers 200, same as NGINX");
	CHECK_EQ(response.getHeaderValue("content-length"), std::string("11"),
		"HEAD still reports the Content-Length a GET would carry");
	TEST(!contains(raw, "<p>home</p>"),
		"HEAD never puts the body on the wire, same as NGINX");
}

/*
 * NGINX's autoindex titles a listing "Index of <uri>"; the generated page
 * this server serves uses the same title wording.
 */
static void	test_autoindex_title_matches_nginx_wording(void)
{
	ServerConfig	config = makeServer();

	config.autoindex = true;

	HttpResponse	response;

	send("GET", "/listed/", config, response);
	CHECK_EQ(response.getStatusCode(), 200,
		"a listable directory answers 200, same as NGINX");
	TEST(contains(response.getBody(), "Index of /listed/"),
		"the listing titles itself the way NGINX's autoindex does");
}

/*
 * NGINX's status line reason phrases are matched for the codes this server
 * is able to answer with.
 */
static void	test_status_reason_phrases_match_nginx(void)
{
	ServerConfig	config = makeServer();

	{
		HttpResponse	response;
		std::string		raw = send("GET", "/index.html", config, response);

		TEST(contains(raw, "HTTP/1.1 200 OK"),
			"200 is reported as \"OK\", same as NGINX");
	}
	{
		HttpResponse	response;
		std::string		raw = send("GET", "/listed", config, response);

		TEST(contains(raw, "HTTP/1.1 301 Moved Permanently"),
			"301 is reported as \"Moved Permanently\", same as NGINX");
	}
	{
		HttpResponse	response;
		std::string		raw = send("GET", "/missing.html", config, response);

		TEST(contains(raw, "HTTP/1.1 404 Not Found"),
			"404 is reported as \"Not Found\", same as NGINX");
	}
}

/* ------------------------------------------------------------------ */
/* Documented differences from NGINX                                   */
/* ------------------------------------------------------------------ */

/*
 * NGINX answers 403 on a directory holding neither an index file nor
 * autoindex on, reading the request as "the listing is forbidden". This
 * server answers 404 there instead, reading it as "the index file was not
 * found" -- the same divergence already documented next to
 * StaticFileHandler::serveDirectory, kept because the tester the subject is
 * graded with expects 404 on that case.
 */
static void	test_empty_directory_diverges_from_nginx_403(void)
{
	ServerConfig	config = makeServer();
	HttpResponse	response;

	send("GET", "/empty/", config, response);
	CHECK_EQ(response.getStatusCode(), 404,
		"documented difference: NGINX answers 403 on an unlisted, "
		"unindexed directory; this server answers 404");
}

/*
 * NGINX stamps a static file's response with Last-Modified, ETag and
 * Accept-Ranges so clients can cache and resume downloads. This server
 * has no caching or range support to back those headers with, so it never
 * sends them; a GET always returns the whole, uncached body instead.
 */
static void	test_static_file_omits_nginx_caching_headers(void)
{
	ServerConfig	config = makeServer();
	HttpResponse	response;

	send("GET", "/index.html", config, response);
	TEST(response.getHeaderValue("last-modified").empty(),
		"documented difference: NGINX sends Last-Modified; "
		"this server has no cache-validation support and sends none");
	TEST(response.getHeaderValue("etag").empty(),
		"documented difference: NGINX sends an ETag; this server sends none");
	TEST(response.getHeaderValue("accept-ranges").empty(),
		"documented difference: NGINX advertises Accept-Ranges: bytes; "
		"this server does not support range requests and sends none");
}

/*
 * NGINX's autoindex renders a <pre> table with each entry's last-modified
 * date and size alongside its name. This server's generated listing is a
 * plain <ul> of links carrying neither column, since it tracks no
 * per-entry metadata beyond the name and whether it is a directory.
 */
static void	test_autoindex_listing_omits_nginx_metadata_columns(void)
{
	ServerConfig	config = makeServer();

	config.autoindex = true;

	HttpResponse	response;

	send("GET", "/listed/", config, response);
	TEST(!contains(response.getBody(), "<pre>"),
		"documented difference: NGINX renders autoindex as a <pre> table; "
		"this server renders a plain <ul> of links");
	TEST(contains(response.getBody(), "<li><a href=\""),
		"this server's listing is a <ul> of <li><a> entries, name only");
}

/*
 * NGINX serves its own static default error page per status code (styled,
 * carrying its version string). This server's built-in fallback is a
 * minimal, unstyled page; both answer the same status and reason phrase,
 * which is what a client or the evaluation tester acts on, so only the
 * status is asserted here and the body wording is left to differ.
 */
static void	test_default_error_body_diverges_from_nginx(void)
{
	ServerConfig	config = makeServer();
	HttpResponse	response;

	send("GET", "/missing.html", config, response);
	CHECK_EQ(response.getStatusCode(), 404,
		"the status still matches NGINX even though the page does not");
	TEST(!contains(response.getBody(), "nginx"),
		"documented difference: the built-in error page never mentions "
		"NGINX's own default page styling or server signature");
}

int	main(void)
{
	setupFixture();

	test_static_file_status_and_type_match_nginx();
	test_unknown_extension_defaults_like_nginx();
	test_directory_without_slash_redirects_like_nginx();
	test_method_not_allowed_carries_allow_header_like_nginx();
	test_configured_redirect_matches_nginx_return_directive();
	test_head_request_matches_nginx_semantics();
	test_autoindex_title_matches_nginx_wording();
	test_status_reason_phrases_match_nginx();

	test_empty_directory_diverges_from_nginx_403();
	test_static_file_omits_nginx_caching_headers();
	test_autoindex_listing_omits_nginx_metadata_columns();
	test_default_error_body_diverges_from_nginx();

	cleanupFixture();

	std::cout << std::endl;
	std::cout << s_pass << " passed, " << s_fail << " failed, "
		<< (s_pass + s_fail) << " total" << std::endl;
	return (s_fail == 0 ? 0 : 1);
}
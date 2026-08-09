/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   autoindex_test.cpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: galves-a <galves-a@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/08/03 19:20:00 by galves-a          #+#    #+#             */
/*   Updated: 2026/08/03 19:20:00 by galves-a         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

# include <cstdio>
# include <cstdlib>
# include <fstream>
# include <iostream>
# include <string>
# include <sys/stat.h>
# include <unistd.h>

# include "config/ConfigLoader.hpp"
# include "handlers/StaticFileHandler.hpp"
# include "http/HttpRequest.hpp"
# include "http/HttpResponse.hpp"
# include "http/Router.hpp"
# include "utils/Utils.hpp"

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

static bool	contains(const std::string &haystack, const std::string &needle)
{
	return (haystack.find(needle) != std::string::npos);
}

/* ------------------------------------------------------------------ */
/* Temporary tree helpers                                              */
/* ------------------------------------------------------------------ */

static std::string	g_root;

static void	writeFile(const std::string &path, const std::string &content)
{
	std::ofstream	out(path.c_str());

	out << content;
	out.close();
}

static void	makeTree(void)
{
	char	tmpl[] = "/tmp/webserv_autoindex_XXXXXX";
	char	*created = mkdtemp(tmpl);

	if (created == NULL)
	{
		std::cerr << "fatal: could not create a temporary directory"
			<< std::endl;
		std::exit(1);
	}
	g_root = created;
	mkdir((g_root + "/listable").c_str(), 0755);
	mkdir((g_root + "/listable/sub dir").c_str(), 0755);
	mkdir((g_root + "/quiet").c_str(), 0755);
	mkdir((g_root + "/loose").c_str(), 0755);
	mkdir((g_root + "/withindex").c_str(), 0755);
	writeFile(g_root + "/listable/plain.txt", "hello");
	writeFile(g_root + "/listable/<img src=x onerror=alert(1)>.txt", "x");
	writeFile(g_root + "/listable/report Q1&Q2.txt", "y");
	writeFile(g_root + "/withindex/index.html", "<p>real index</p>");
	writeFile(g_root + "/withindex/guide.html", "<p>guide</p>");
}

static void	removeTree(void)
{
	std::string	cmd = "rm -rf '" + g_root + "'";

	if (system(cmd.c_str()) != 0)
		std::cerr << "warning: could not clean " << g_root << std::endl;
}

static HttpResponse	get(StaticFileHandler &handler, const std::string &uri)
{
	HttpRequest		request;
	HttpResponse	response;

	request.setMethod("GET");
	request.setUri(uri);
	handler.handle(request, response);
	return (response);
}

/* ------------------------------------------------------------------ */
/* Escaping helpers                                                    */
/* ------------------------------------------------------------------ */

static void	test_html_escape(void)
{
	CHECK_EQ(htmlEscape("<img src=x>"), std::string("&lt;img src=x&gt;"),
	         "htmlEscape neutralises angle brackets");
	CHECK_EQ(htmlEscape("Q1&Q2"), std::string("Q1&amp;Q2"),
	         "htmlEscape escapes ampersands");
	CHECK_EQ(htmlEscape("a\"b'c"), std::string("a&quot;b&#39;c"),
	         "htmlEscape escapes both quote styles");
	CHECK_EQ(htmlEscape("plain.txt"), std::string("plain.txt"),
	         "htmlEscape leaves ordinary names untouched");
}

static void	test_url_encode(void)
{
	CHECK_EQ(urlEncodePath("/a b/c"), std::string("/a%20b/c"),
	         "urlEncodePath keeps '/' and encodes spaces");
	CHECK_EQ(urlEncodeSegment("a/b"), std::string("a%2Fb"),
	         "urlEncodeSegment escapes '/' so a name cannot add a segment");
	CHECK_EQ(urlEncodeSegment("q?x#y"), std::string("q%3Fx%23y"),
	         "urlEncodeSegment escapes query and fragment delimiters");
	CHECK_EQ(urlEncodePath("/ok-file_1.2~x"), std::string("/ok-file_1.2~x"),
	         "urlEncodePath leaves unreserved characters untouched");
}

/* ------------------------------------------------------------------ */
/* Listing behaviour                                                   */
/* ------------------------------------------------------------------ */

static void	test_autoindex_off_is_404(void)
{
	StaticFileHandler	handler(g_root);

	handler.setAutoindex(false);

	HttpResponse	response = get(handler, "/quiet/");

	CHECK_EQ(response.getStatusCode(), 404,
	         "directory without index and autoindex off returns 404");
}

static void	test_autoindex_on_lists_entries(void)
{
	StaticFileHandler	handler(g_root);

	handler.setAutoindex(true);

	HttpResponse	response = get(handler, "/listable/");

	CHECK_EQ(response.getStatusCode(), 200,
	         "directory without index and autoindex on returns 200");
	CHECK_EQ(response.getHeaderValue("content-type"), std::string("text/html"),
	         "generated listing is typed text/html");
	TEST(contains(response.getBody(), "Index of /listable/"),
	     "listing carries the requested URI as its title");
	TEST(contains(response.getBody(), ">plain.txt</a>"),
	     "listing links every regular entry");
	TEST(!contains(response.getBody(), ">.</a>")
	     && !contains(response.getBody(), ">..</a>"),
	     "listing drops the '.' and '..' entries");
}

static void	test_autoindex_empty_directory(void)
{
	StaticFileHandler	handler(g_root);

	handler.setAutoindex(true);

	HttpResponse	response = get(handler, "/quiet/");

	CHECK_EQ(response.getStatusCode(), 200,
	         "empty directory with autoindex on still returns a listing");
	TEST(contains(response.getBody(), "<ul>"),
	     "empty listing is still a well formed page");
}

static void	test_index_wins_over_autoindex(void)
{
	StaticFileHandler	handler(g_root);

	handler.setAutoindex(true);

	HttpResponse	response = get(handler, "/withindex/");

	TEST(contains(response.getBody(), "real index"),
	     "an existing index file is served instead of a listing");
}

static void	test_configured_index_wins_over_autoindex(void)
{
	StaticFileHandler	handler(g_root);

	handler.setAutoindex(true);
	handler.setIndex("guide.html");

	HttpResponse	response = get(handler, "/withindex/");

	TEST(contains(response.getBody(), "guide"),
	     "the configured index name is honoured before autoindex");
}

/* ------------------------------------------------------------------ */
/* Escaping applied to the generated page                              */
/* ------------------------------------------------------------------ */

static void	test_entry_names_are_escaped(void)
{
	StaticFileHandler	handler(g_root);

	handler.setAutoindex(true);

	std::string	body = get(handler, "/listable/").getBody();

	TEST(!contains(body, "<img src=x onerror=alert(1)>"),
	     "a hostile file name is never emitted as live markup");
	TEST(contains(body, "&lt;img src=x onerror=alert(1)&gt;.txt"),
	     "a hostile file name is emitted escaped");
	TEST(contains(body, "report Q1&amp;Q2.txt"),
	     "an ampersand in a file name is emitted as an entity");
	TEST(contains(body, "report%20Q1%26Q2.txt"),
	     "an entry href is percent-encoded");
}

static void	test_request_uri_is_escaped(void)
{
	StaticFileHandler	handler(g_root);

	handler.setAutoindex(true);

	std::string	uri = "/listable/<img src=x onerror=alert(1)>/../";
	std::string	body = get(handler, uri).getBody();

	TEST(!contains(body, "<img src=x onerror=alert(1)>"),
	     "a payload smuggled through the URI is not reflected as markup");
	TEST(contains(body, "&lt;img src=x onerror=alert(1)&gt;"),
	     "a payload smuggled through the URI is reflected escaped");
}

static void	test_directory_entries_keep_trailing_slash(void)
{
	StaticFileHandler	handler(g_root);

	handler.setAutoindex(true);

	std::string	body = get(handler, "/listable/").getBody();

	TEST(contains(body, "href=\"/listable/sub%20dir/\""),
	     "a directory entry links with a trailing slash");
	TEST(contains(body, ">sub dir/</a>"),
	     "a directory entry is labelled with a trailing slash");
}

/* ------------------------------------------------------------------ */
/* Configuration plumbing                                              */
/* ------------------------------------------------------------------ */

static std::vector<ServerConfig>	loadConf(const std::string &body)
{
	std::string	path = g_root + "/test.conf";

	writeFile(path, body);

	ConfigLoader	loader(path);

	return (loader.loader());
}

static void	test_location_autoindex_is_parsed(void)
{
	std::vector<ServerConfig>	servers = loadConf(
		"server {\n listen 8080;\n"
		" location /on { autoindex on; }\n"
		" location /off { autoindex off; }\n"
		" location /bare { }\n}\n");

	CHECK_EQ(servers.size(), static_cast<size_t>(1), "one server parsed");
	CHECK_EQ(servers[0].locations.size(), static_cast<size_t>(3),
	         "three locations parsed");
	CHECK_EQ(servers[0].locations[0].autoindex, true,
	         "'autoindex on' in a location is parsed as true");
	CHECK_EQ(servers[0].locations[1].autoindex, false,
	         "'autoindex off' in a location is parsed as false");
	CHECK_EQ(servers[0].locations[2].autoindex, false,
	         "a location without autoindex defaults to off");
}

static void	test_server_autoindex_is_inherited(void)
{
	std::vector<ServerConfig>	servers = loadConf(
		"server {\n listen 8080;\n autoindex on;\n"
		" location /inherits { }\n"
		" location /overrides { autoindex off; }\n}\n");

	CHECK_EQ(servers[0].autoindex, true,
	         "'autoindex on' at server level is parsed");
	CHECK_EQ(servers[0].locations[0].autoindex, true,
	         "a location without autoindex inherits the server value");
	CHECK_EQ(servers[0].locations[1].autoindex, false,
	         "a location overrides the inherited server value");
}

static void	test_invalid_autoindex_is_refused(void)
{
	bool	threw = false;

	try
	{
		loadConf("server {\n listen 8080;\n autoindex maybe;\n}\n");
	}
	catch (const std::exception &)
	{
		threw = true;
	}
	TEST(threw, "an autoindex value other than on/off is refused");
}

/* ------------------------------------------------------------------ */
/* Router wiring: config reaches the handler, and never leaks          */
/* ------------------------------------------------------------------ */

static ServerConfig	routerConfig(bool serverAutoindex)
{
	ServerConfig	config;
	LocationConfig	on("/listable");
	LocationConfig	off("/quiet");

	config.root = g_root;
	config.index = "index.html";
	config.autoindex = serverAutoindex;
	on.autoindex = true;
	on.index = "index.html";
	off.autoindex = false;
	off.index = "index.html";
	config.locations.push_back(on);
	config.locations.push_back(off);
	return (config);
}

static int	routeStatus(Router &router, const ServerConfig &config,
		const std::string &uri)
{
	HttpRequest		request;
	HttpResponse	response;

	request.setMethod("GET");
	request.setUri(uri);
	router.route(request, response, config);
	return (response.getStatusCode());
}

static void	test_router_applies_location_autoindex(void)
{
	Router			router(g_root);
	ServerConfig	config = routerConfig(false);

	CHECK_EQ(routeStatus(router, config, "/listable/"), 200,
	         "router enables autoindex for a location that declares it on");
	CHECK_EQ(routeStatus(router, config, "/quiet/"), 404,
	         "autoindex does not leak from the previous request");
	CHECK_EQ(routeStatus(router, config, "/listable/"), 200,
	         "the listable location still lists after a request that did not");
}

static void	test_router_inherits_server_autoindex(void)
{
	Router			router(g_root);
	ServerConfig	config = routerConfig(true);

	CHECK_EQ(routeStatus(router, config, "/loose/"), 200,
	         "a URI matching no location inherits the server autoindex");
	CHECK_EQ(routeStatus(router, config, "/quiet/"), 404,
	         "a location declaring autoindex off overrides the server value");
}

int	main(void)
{
	std::cout << "=== autoindex tests ===" << std::endl;

	makeTree();

	test_html_escape();
	test_url_encode();
	test_autoindex_off_is_404();
	test_autoindex_on_lists_entries();
	test_autoindex_empty_directory();
	test_index_wins_over_autoindex();
	test_configured_index_wins_over_autoindex();
	test_entry_names_are_escaped();
	test_request_uri_is_escaped();
	test_directory_entries_keep_trailing_slash();
	test_location_autoindex_is_parsed();
	test_server_autoindex_is_inherited();
	test_invalid_autoindex_is_refused();
	test_router_applies_location_autoindex();
	test_router_inherits_server_autoindex();

	removeTree();

	std::cout << "--- " << s_pass << "/" << s_test_num << " passed ---"
		<< std::endl;
	return (s_fail == 0 ? 0 : 1);
}

/******************************************************************************/
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   mime_types_test.cpp                                :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dajesus- <dajesus-@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/12 21:00:08 by dajesus-          #+#    #+#             */
/*   Updated: 2026/06/12 22:30:33 by dajesus-         ###   ########.fr       */
/*                                                                            */
/******************************************************************************/

# include <iostream>
# include <string>

# include "http/MimeType.hpp"
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

/* ------------------------------------------------------------------ */
/* MIME Type tests                                                     */
/* ------------------------------------------------------------------ */

static void	test_mime_known_types(void)
{
	CHECK_EQ(mimeType_resolve("index.html"), "text/html",
	         "html -> text/html");
	CHECK_EQ(mimeType_resolve("style.css"), "text/css",
	         "css -> text/css");
	CHECK_EQ(mimeType_resolve("app.js"), "application/javascript",
	         "js -> application/javascript");
	CHECK_EQ(mimeType_resolve("data.json"), "application/json",
	         "json -> application/json");
	CHECK_EQ(mimeType_resolve("config.xml"), "application/xml",
	         "xml -> application/xml");

	CHECK_EQ(mimeType_resolve("photo.png"), "image/png",
	         "png -> image/png");
	CHECK_EQ(mimeType_resolve("photo.jpg"), "image/jpeg",
	         "jpg -> image/jpeg");
	CHECK_EQ(mimeType_resolve("photo.jpeg"), "image/jpeg",
	         "jpeg -> image/jpeg");
	CHECK_EQ(mimeType_resolve("icon.gif"), "image/gif",
	         "gif -> image/gif");
	CHECK_EQ(mimeType_resolve("logo.svg"), "image/svg+xml",
	         "svg -> image/svg+xml");
	CHECK_EQ(mimeType_resolve("favicon.ico"), "image/x-icon",
	         "ico -> image/x-icon");
	CHECK_EQ(mimeType_resolve("photo.webp"), "image/webp",
	         "webp -> image/webp");

	CHECK_EQ(mimeType_resolve("readme.txt"), "text/plain",
	         "txt -> text/plain");
	CHECK_EQ(mimeType_resolve("data.csv"), "text/csv",
	         "csv -> text/csv");
	CHECK_EQ(mimeType_resolve("notes.md"), "text/markdown",
	         "md -> text/markdown");
	CHECK_EQ(mimeType_resolve("manual.pdf"), "application/pdf",
	         "pdf -> application/pdf");

	CHECK_EQ(mimeType_resolve("page.htm"), "text/html",
	         "htm (no l) -> text/html");
}

static void	test_mime_case_insensitive(void)
{
	CHECK_EQ(mimeType_resolve("index.HTML"), "text/html",
	         ".HTML uppercase -> text/html");
	CHECK_EQ(mimeType_resolve("style.CSS"), "text/css",
	         ".CSS uppercase -> text/css");
	CHECK_EQ(mimeType_resolve("app.JS"), "application/javascript",
	         ".JS uppercase -> application/javascript");
	CHECK_EQ(mimeType_resolve("photo.PNG"), "image/png",
	         ".PNG uppercase -> image/png");
	CHECK_EQ(mimeType_resolve("photo.JPG"), "image/jpeg",
	         ".JPG uppercase -> image/jpeg");

	CHECK_EQ(mimeType_resolve("file.HtMl"), "text/html",
	         ".HtMl mixed case -> text/html");
}

static void	test_mime_fallback(void)
{
	/* Sem extensão */
	CHECK_EQ(mimeType_resolve("Makefile"), "application/octet-stream",
	         "no extension (Makefile) -> octet-stream");
	CHECK_EQ(mimeType_resolve("README"), "application/octet-stream",
	         "no extension (README) -> octet-stream");
	CHECK_EQ(mimeType_resolve("Dockerfile"), "application/octet-stream",
	         "no extension (Dockerfile) -> octet-stream");

	/* Só ponto no início */
	CHECK_EQ(mimeType_resolve(".gitignore"), "application/octet-stream",
	         "dot-only leading file (.gitignore) -> octet-stream");
	CHECK_EQ(mimeType_resolve(".env"), "application/octet-stream",
	         "dot-only leading file (.env) -> octet-stream");

	/* Ponto no final vazio */
	CHECK_EQ(mimeType_resolve("file."), "application/octet-stream",
	         "trailing dot with empty ext -> octet-stream");

	/* Extensão desconhecida */
	CHECK_EQ(mimeType_resolve("script.php"), "application/octet-stream",
	         "unknown extension (php) -> octet-stream");
	CHECK_EQ(mimeType_resolve("archive.zip"), "application/octet-stream",
	         "unknown extension (zip) -> octet-stream");
}

static void	test_mime_edge_cases(void)
{
	/* Múltiplos pontos — pega o último */
	CHECK_EQ(mimeType_resolve("a.b.c.html"), "text/html",
	         "multi-dot path uses last ext");
	CHECK_EQ(mimeType_resolve("/path/to/style.CSS.minified"), "application/octet-stream",
	         "complex path — last ext (.minified) is unknown");

	/* Caminho absoluto */
	CHECK_EQ(mimeType_resolve("/var/www/index.html"), "text/html",
	         "absolute path html -> text/html");
	CHECK_EQ(mimeType_resolve("/uploads/photo.PNG"), "image/png",
	         "absolute path + uppercase ext");

	/* String vazia */
	CHECK_EQ(mimeType_resolve(""), "application/octet-stream",
	         "empty string -> octet-stream");

	/* Só um ponto */
	CHECK_EQ(mimeType_resolve("."), "application/octet-stream",
	         "single dot -> octet-stream");
}

static void	test_utils_toLower(void)
{
	/* Casos normais ASCII */
	CHECK_EQ(toLower("HELLO"), "hello",
	         "ascii uppercase -> lowercase");
	CHECK_EQ(toLower("Hello World"), "hello world",
	         "mixed case with space");
	CHECK_EQ(toLower("already_lower"), "already_lower",
	         "already lowercase");

	/* Bytes altos — verifica que não há UB (não crasha) */
	std::string high("UPPER\xffLOWER");
	std::string result = toLower(high);
	TEST(result.size() == high.size(),
	     "toLower preserves size with high bytes");
}

int	main(void)
{
	test_mime_known_types();
	test_mime_case_insensitive();
	test_mime_fallback();
	test_mime_edge_cases();
	test_utils_toLower();

	std::cout << std::endl;
	std::cout << s_pass << " passed, " << s_fail << " failed, "
	          << (s_pass + s_fail) << " total" << std::endl;

	return (s_fail == 0 ? 0 : 1);
}

/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   upload_suite_test.cpp                              :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dajesus- <dajesus-@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/30 23:11:19 by dajesus-          #+#    #+#             */
/*   Updated: 2026/07/31 01:30:37 by dajesus-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <iostream>
#include <fstream>
#include <string>
#include <cstdio>
#include <sys/stat.h>
#include <vector>

#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"
#include "http/MultipartParser.hpp"
#include "handlers/StaticFileHandler.hpp"
#include "handlers/IRequestHandler.hpp"

/* ------------------------------------------------------------------ */
/* Minimal test framework (C++98, zero dependencies)                   */
/* ------------------------------------------------------------------ */

static int	s_pass = 0;
static int	s_fail = 0;

# define TEST(cond, name) \
	do { \
		if (cond) { s_pass++; std::cout << "[PASS] " << name << std::endl; } \
		else { s_fail++; std::cerr << "[FAIL] " << name << std::endl; } \
	} while (0)

/* ------------------------------------------------------------------ */
/* Helpers                                                              */
/* ------------------------------------------------------------------ */

static std::string	readFile(const std::string &path)
{
	std::ifstream	file(path.c_str());

	if (!file.is_open())
		return ("");
	return (std::string((std::istreambuf_iterator<char>(file)),
			std::istreambuf_iterator<char>()));
}

static std::string	generateBinaryContent(std::size_t size)
{
	std::string	result;

	result.reserve(size);
	for (std::size_t i = 0; i < size; ++i)
		result.push_back(static_cast<char>((i % 256) - 128));
	return (result);
}

static void	createDirectoryTree(const std::string &root)
{
	mkdir(root.c_str(), 0755);
	mkdir((root + "/uploads").c_str(), 0755);
	mkdir((root + "/uploads/deep").c_str(), 0755);
}

static void	destroyDirectoryTree(const std::string &root)
{
	std::remove((root + "/uploads/deep/test.txt").c_str());
	std::remove((root + "/uploads/plain.txt").c_str());
	std::remove((root + "/uploads/binary.bin").c_str());
	std::remove((root + "/uploads/empty.txt").c_str());
	std::remove((root + "/uploads/big.txt").c_str());
	std::remove((root + "/uploads/overwrite.txt").c_str());
	std::remove((root + "/uploads/a.txt").c_str());
	std::remove((root + "/uploads/b.txt").c_str());
	rmdir((root + "/uploads/deep").c_str());
	rmdir((root + "/uploads").c_str());
	rmdir(root.c_str());
}

/* ------------------------------------------------------------------ */
/* Plain POST upload                                                    */
/* ------------------------------------------------------------------ */

static void	test_plain_post_creates_file(void)
{
	createDirectoryTree("up_root");

	StaticFileHandler	handler("up_root");
	HttpRequest		request;
	HttpResponse	response;

	request.setMethod("POST");
	request.setUri("/uploads/plain.txt");
	request.setBody("plain text body");

	bool	handled = handler.handle(request, response);

	TEST(handled, "plain POST returns handled");
	TEST(response.getStatusCode() == 201,
		"plain POST answers 201 for a new file");
	TEST(readFile("up_root/uploads/plain.txt") == "plain text body",
		"plain POST writes exact body content to file");

	destroyDirectoryTree("up_root");
}

static void	test_plain_post_overwrites_file(void)
{
	createDirectoryTree("up_root");

	std::ofstream	existing("up_root/uploads/overwrite.txt");
	existing << "original content";
	existing.close();

	StaticFileHandler	handler("up_root");
	HttpRequest		request;
	HttpResponse	response;

	request.setMethod("POST");
	request.setUri("/uploads/overwrite.txt");
	request.setBody("replacement content");

	handler.handle(request, response);

	TEST(response.getStatusCode() == 200,
		"plain POST answers 200 when overwriting");
	TEST(readFile("up_root/uploads/overwrite.txt") == "replacement content",
		"plain POST replaces previous content");

	destroyDirectoryTree("up_root");
}

/* ------------------------------------------------------------------ */
/* Binary content integrity                                             */
/* ------------------------------------------------------------------ */

static void	test_plain_post_preserves_binary_content(void)
{
	createDirectoryTree("up_root");

	StaticFileHandler	handler("up_root");
	HttpRequest		request;
	HttpResponse	response;
	std::string		original = generateBinaryContent(1024);

	request.setMethod("POST");
	request.setUri("/uploads/binary.bin");
	request.setBody(original);

	handler.handle(request, response);

	TEST(response.getStatusCode() == 201,
		"binary upload answers 201");
	TEST(readFile("up_root/uploads/binary.bin") == original,
		"binary upload preserves all bytes exactly");

	destroyDirectoryTree("up_root");
}

static void	test_multipart_preserves_binary_content(void)
{
	createDirectoryTree("up_root");

	StaticFileHandler	handler("up_root");
	HttpRequest		request;
	HttpResponse	response;
	std::string		payload = generateBinaryContent(512);
	std::string		boundary = "BOUNDARY";
	std::string		body =
		"--BOUNDARY\r\n"
		"Content-Disposition: form-data; name=\"file\"; filename=\"binary.bin\"\r\n"
		"Content-Type: application/octet-stream\r\n"
		"\r\n";
	body += payload;
	body += "\r\n--BOUNDARY--\r\n";

	request.setMethod("POST");
	request.setUri("/uploads");
	request.setHeaders("Content-Type",
		"multipart/form-data; boundary=BOUNDARY");
	request.setBody(body);

	handler.handle(request, response);

	TEST(response.getStatusCode() == 201,
		"multipart binary upload answers 201");
	TEST(readFile("up_root/uploads/binary.bin") == payload,
		"multipart binary upload preserves all bytes exactly");

	destroyDirectoryTree("up_root");
}

/* ------------------------------------------------------------------ */
/* Empty content handling                                               */
/* ------------------------------------------------------------------ */

static void	test_plain_post_accepts_empty_body(void)
{
	createDirectoryTree("up_root");

	StaticFileHandler	handler("up_root");
	HttpRequest		request;
	HttpResponse	response;

	request.setMethod("POST");
	request.setUri("/uploads/empty.txt");
	request.setBody("");

	handler.handle(request, response);

	TEST(response.getStatusCode() == 201,
		"empty body upload answers 201");
	TEST(readFile("up_root/uploads/empty.txt") == "",
		"empty body upload creates an empty file");

	destroyDirectoryTree("up_root");
}

/* ------------------------------------------------------------------ */
/* Large content upload                                                 */
/* ------------------------------------------------------------------ */

static void	test_plain_post_handles_large_body(void)
{
	createDirectoryTree("up_root");

	StaticFileHandler	handler("up_root");
	handler.setMaxBodySize(1 * 1024 * 1024);
	HttpRequest		request;
	HttpResponse	response;
	std::string		content = generateBinaryContent(100 * 1024);

	request.setMethod("POST");
	request.setUri("/uploads/big.txt");
	request.setBody(content);

	handler.handle(request, response);

	TEST(response.getStatusCode() == 201,
		"large body upload answers 201");
	TEST(readFile("up_root/uploads/big.txt") == content,
		"large body upload saves complete content");

	destroyDirectoryTree("up_root");
}

/* ------------------------------------------------------------------ */
/* Upload respects maxBodySize                                          */
/* ------------------------------------------------------------------ */

static void	test_plain_post_rejects_oversized(void)
{
	createDirectoryTree("up_root");

	StaticFileHandler	handler("up_root");
	handler.setMaxBodySize(10);
	HttpRequest		request;
	HttpResponse	response;

	request.setMethod("POST");
	request.setUri("/uploads/rejected.txt");
	request.setBody("this body is longer than ten bytes");

	handler.handle(request, response);

	TEST(response.getStatusCode() == 413,
		"oversized plain POST answers 413");
	TEST(readFile("up_root/uploads/rejected.txt") == "",
		"oversized plain POST leaves no file behind");

	destroyDirectoryTree("up_root");
}

/* ------------------------------------------------------------------ */
/* Upload to subdirectory                                               */
/* ------------------------------------------------------------------ */

static void	test_plain_post_to_subdirectory(void)
{
	createDirectoryTree("up_root");

	StaticFileHandler	handler("up_root");
	HttpRequest		request;
	HttpResponse	response;

	request.setMethod("POST");
	request.setUri("/uploads/deep/test.txt");
	request.setBody("deep content");

	handler.handle(request, response);

	TEST(response.getStatusCode() == 201,
		"upload to subdirectory answers 201");
	TEST(readFile("up_root/uploads/deep/test.txt") == "deep content",
		"upload to subdirectory saves file in correct path");

	destroyDirectoryTree("up_root");
}

/* ------------------------------------------------------------------ */
/* Multipart: multiple file parts                                       */
/* ------------------------------------------------------------------ */

static void	test_multipart_multiple_files(void)
{
	createDirectoryTree("up_root");

	StaticFileHandler	handler("up_root");
	HttpRequest		request;
	HttpResponse	response;
	std::string		boundary = "BOUNDARY";
	std::string		body =
		"--BOUNDARY\r\n"
		"Content-Disposition: form-data; name=\"file1\"; filename=\"a.txt\"\r\n"
		"\r\n"
		"content-a"
		"\r\n--BOUNDARY\r\n"
		"Content-Disposition: form-data; name=\"file2\"; filename=\"b.txt\"\r\n"
		"\r\n"
		"content-b"
		"\r\n--BOUNDARY--\r\n";

	request.setMethod("POST");
	request.setUri("/uploads");
	request.setHeaders("Content-Type",
		"multipart/form-data; boundary=BOUNDARY");
	request.setBody(body);

	handler.handle(request, response);

	TEST(response.getStatusCode() == 201,
		"multipart with two files answers 201");
	TEST(readFile("up_root/uploads/a.txt") == "content-a",
		"first file saved with correct content");
	TEST(readFile("up_root/uploads/b.txt") == "content-b",
		"second file saved with correct content");

	destroyDirectoryTree("up_root");
}

/* ------------------------------------------------------------------ */
/* Multipart: overwrite when at least one file exists                   */
/* ------------------------------------------------------------------ */

static void	test_multipart_mixed_new_and_existing(void)
{
	createDirectoryTree("up_root");

	std::ofstream	existing("up_root/uploads/a.txt");
	existing << "old";
	existing.close();

	StaticFileHandler	handler("up_root");
	HttpRequest		request;
	HttpResponse	response;
	std::string		boundary = "BOUNDARY";
	std::string		body =
		"--BOUNDARY\r\n"
		"Content-Disposition: form-data; name=\"file1\"; filename=\"a.txt\"\r\n"
		"\r\n"
		"new-a"
		"\r\n--BOUNDARY\r\n"
		"Content-Disposition: form-data; name=\"file2\"; filename=\"b.txt\"\r\n"
		"\r\n"
		"new-b"
		"\r\n--BOUNDARY--\r\n";

	request.setMethod("POST");
	request.setUri("/uploads");
	request.setHeaders("Content-Type",
		"multipart/form-data; boundary=BOUNDARY");
	request.setBody(body);

	handler.handle(request, response);

	TEST(response.getStatusCode() == 201,
		"mixed new and existing files answers 201 when any file is new");
	TEST(readFile("up_root/uploads/a.txt") == "new-a",
		"existing file content is overwritten");
	TEST(readFile("up_root/uploads/b.txt") == "new-b",
		"new file content is saved");

	destroyDirectoryTree("up_root");
}

/* ------------------------------------------------------------------ */
/* Multipart: all files exist (200)                                     */
/* ------------------------------------------------------------------ */

static void	test_multipart_all_existing(void)
{
	createDirectoryTree("up_root");

	std::ofstream	f1("up_root/uploads/a.txt");
	f1 << "old-a";
	f1.close();
	std::ofstream	f2("up_root/uploads/b.txt");
	f2 << "old-b";
	f2.close();

	StaticFileHandler	handler("up_root");
	HttpRequest		request;
	HttpResponse	response;
	std::string		boundary = "BOUNDARY";
	std::string		body =
		"--BOUNDARY\r\n"
		"Content-Disposition: form-data; name=\"file1\"; filename=\"a.txt\"\r\n"
		"\r\n"
		"new-a"
		"\r\n--BOUNDARY\r\n"
		"Content-Disposition: form-data; name=\"file2\"; filename=\"b.txt\"\r\n"
		"\r\n"
		"new-b"
		"\r\n--BOUNDARY--\r\n";

	request.setMethod("POST");
	request.setUri("/uploads");
	request.setHeaders("Content-Type",
		"multipart/form-data; boundary=BOUNDARY");
	request.setBody(body);

	handler.handle(request, response);

	TEST(response.getStatusCode() == 200,
		"multipart with all existing files answers 200");
	TEST(readFile("up_root/uploads/a.txt") == "new-a",
		"all files overwritten correctly (a)");
	TEST(readFile("up_root/uploads/b.txt") == "new-b",
		"all files overwritten correctly (b)");

	destroyDirectoryTree("up_root");
}

/* ------------------------------------------------------------------ */
/* Upload directory validation: file exists after handler returns       */
/* ------------------------------------------------------------------ */

static void	test_uploaded_file_exists_on_disk(void)
{
	createDirectoryTree("up_root");

	StaticFileHandler	handler("up_root");
	HttpRequest		request;
	HttpResponse	response;

	request.setMethod("POST");
	request.setUri("/uploads/plain.txt");
	request.setBody("verify on disk");

	handler.handle(request, response);

	struct stat	st;
	int			exists = (stat("up_root/uploads/plain.txt", &st) == 0);

	TEST(exists, "uploaded file exists on filesystem");
	TEST(S_ISREG(st.st_mode),
		"uploaded file is a regular file");
	TEST(st.st_size == static_cast<off_t>(14),
		"uploaded file has correct size");

	destroyDirectoryTree("up_root");
}

/* ------------------------------------------------------------------ */
/* Non-POST method does not trigger upload                              */
/* ------------------------------------------------------------------ */

static void	test_get_des_not_upload(void)
{
	createDirectoryTree("up_root");

	std::ofstream	f("up_root/uploads/readonly.txt");
	f << "existing data";
	f.close();

	StaticFileHandler	handler("up_root");
	HttpRequest		request;
	HttpResponse	response;

	request.setMethod("GET");
	request.setUri("/uploads/readonly.txt");

	handler.handle(request, response);

	TEST(response.getStatusCode() == 200,
		"GET answers 200 (serves file, does not alter it)");
	TEST(readFile("up_root/uploads/readonly.txt") == "existing data",
		"GET does not modify the file content");

	destroyDirectoryTree("up_root");
}

/* ------------------------------------------------------------------ */
/* Upload to non-existent base directory                                */
/* ------------------------------------------------------------------ */

static void	test_upload_to_missing_directory(void)
{
	mkdir("up_orphan", 0755);

	StaticFileHandler	handler("up_orphan");
	HttpRequest		request;
	HttpResponse	response;

	request.setMethod("POST");
	request.setUri("/nonexistent/file.txt");
	request.setBody("lost");

	handler.handle(request, response);

	TEST(response.getStatusCode() == 404,
		"upload to missing directory answers 404");

	rmdir("up_orphan");
}

/* ------------------------------------------------------------------ */
/* Upload permissions / read-only directory                             */
/* ------------------------------------------------------------------ */

static void	test_upload_to_readonly_parent(void)
{
	mkdir("up_ro", 0755);
	mkdir("up_ro/nope", 0555);

	StaticFileHandler	handler("up_ro");
	HttpRequest		request;
	HttpResponse	response;

	request.setMethod("POST");
	request.setUri("/nope/file.txt");
	request.setBody("blocked");

	handler.handle(request, response);

	TEST(response.getStatusCode() == 403,
		"upload to read-only directory answers 403");

	rmdir("up_ro/nope");
	rmdir("up_ro");
}

int	main(void)
{
	test_plain_post_creates_file();
	test_plain_post_overwrites_file();
	test_plain_post_preserves_binary_content();
	test_multipart_preserves_binary_content();
	test_plain_post_accepts_empty_body();
	test_plain_post_handles_large_body();
	test_plain_post_rejects_oversized();
	test_plain_post_to_subdirectory();
	test_multipart_multiple_files();
	test_multipart_mixed_new_and_existing();
	test_multipart_all_existing();
	test_uploaded_file_exists_on_disk();
	test_get_des_not_upload();
	test_upload_to_missing_directory();
	test_upload_to_readonly_parent();
	std::cout << std::endl << s_pass << " passed, " << s_fail
		<< " failed" << std::endl;
	return (s_fail == 0 ? 0 : 1);
}

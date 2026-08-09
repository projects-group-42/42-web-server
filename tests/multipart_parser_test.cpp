/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   multipart_parser_test.cpp                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dajesus- <dajesus-@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/24 16:22:35 by dajesus-          #+#    #+#             */
/*   Updated: 2026/07/24 18:21:39 by dajesus-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

# include <iostream>
# include <fstream>
# include <string>
# include <cstdio>
# include <sys/stat.h>

# include "http/MultipartParser.hpp"
# include "handlers/StaticFileHandler.hpp"
# include "http/HttpRequest.hpp"
# include "http/HttpResponse.hpp"

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
/* extractBoundary                                                     */
/* ------------------------------------------------------------------ */

static void	test_extract_boundary(void)
{
	std::string	boundary;

	TEST(MultipartParser::extractBoundary(
			"multipart/form-data; boundary=----WebKitBoundaryABC", boundary)
		&& boundary == "----WebKitBoundaryABC",
		"extracts an unquoted boundary");

	TEST(MultipartParser::extractBoundary(
			"multipart/form-data; boundary=\"abc123\"", boundary)
		&& boundary == "abc123",
		"extracts a quoted boundary");

	TEST(!MultipartParser::extractBoundary("multipart/form-data", boundary)
		&& boundary.empty(),
		"reports missing boundary parameter");

	TEST(MultipartParser::extractBoundary(
			"multipart/form-data; myboundary=WRONG; boundary=RIGHT", boundary)
		&& boundary == "RIGHT",
		"ignores a substring match inside another parameter");

	TEST(!MultipartParser::extractBoundary(
			"multipart/form-data; myboundary=WRONG", boundary)
		&& boundary.empty(),
		"reports missing boundary when only a substring match exists");
}

/* ------------------------------------------------------------------ */
/* parse                                                                */
/* ------------------------------------------------------------------ */

static void	test_parse_single_file(void)
{
	std::string	boundary = "BOUND";
	std::string	body =
		"--BOUND\r\n"
		"Content-Disposition: form-data; name=\"file\"; filename=\"a.txt\"\r\n"
		"Content-Type: text/plain\r\n"
		"\r\n"
		"hello world"
		"\r\n--BOUND--\r\n";

	MultipartParser	parser;
	bool			ok = parser.parse(body, boundary);

	TEST(ok, "parses a single-file body");
	TEST(parser.getParts().size() == 1, "extracts exactly one part");
	if (parser.getParts().size() == 1)
	{
		const MultipartPart	&part = parser.getParts()[0];
		TEST(part.name == "file", "extracts the field name");
		TEST(part.filename == "a.txt", "extracts the filename");
		TEST(part.content == "hello world", "extracts the file content");
		TEST(part.isFile(), "flags the part as a file");
	}
}

static void	test_parse_mixed_fields(void)
{
	std::string	boundary = "XYZ";
	std::string	body =
		"--XYZ\r\n"
		"Content-Disposition: form-data; name=\"caption\"\r\n"
		"\r\n"
		"a picture"
		"\r\n--XYZ\r\n"
		"Content-Disposition: form-data; name=\"photo\"; filename=\"pic.png\"\r\n"
		"Content-Type: image/png\r\n"
		"\r\n"
		"\x89PNG..data"
		"\r\n--XYZ--\r\n";

	MultipartParser	parser;
	bool			ok = parser.parse(body, boundary);

	TEST(ok, "parses a body mixing a text field and a file field");
	TEST(parser.getParts().size() == 2, "extracts both parts");
	if (parser.getParts().size() == 2)
	{
		TEST(!parser.getParts()[0].isFile(), "text field is not a file");
		TEST(parser.getParts()[1].isFile(), "second part is a file");
		TEST(parser.getParts()[1].filename == "pic.png",
			"keeps the filename of the second part");
	}
}

static void	test_parse_rejects_missing_boundary(void)
{
	MultipartParser	parser;
	bool			ok = parser.parse("whatever", "");

	TEST(!ok, "rejects an empty boundary");
	TEST(parser.getErrorCode() == 400, "reports 400 for an empty boundary");
}

static void	test_parse_rejects_truncated_body(void)
{
	std::string	boundary = "BOUND";
	std::string	body =
		"--BOUND\r\n"
		"Content-Disposition: form-data; name=\"file\"; filename=\"a.txt\"\r\n"
		"\r\n"
		"unterminated";

	MultipartParser	parser;
	bool			ok = parser.parse(body, boundary);

	TEST(!ok, "rejects a body missing the closing boundary");
	TEST(parser.getErrorCode() == 400, "reports 400 for a truncated body");
}

static void	test_parse_rejects_part_without_disposition(void)
{
	std::string	boundary = "BOUND";
	std::string	body =
		"--BOUND\r\n"
		"Content-Type: text/plain\r\n"
		"\r\n"
		"no disposition header"
		"\r\n--BOUND--\r\n";

	MultipartParser	parser;
	bool			ok = parser.parse(body, boundary);

	TEST(!ok, "rejects a part without Content-Disposition");
	TEST(parser.getErrorCode() == 400,
		"reports 400 for a missing Content-Disposition header");
}

/* ------------------------------------------------------------------ */
/* StaticFileHandler integration                                       */
/* ------------------------------------------------------------------ */

static bool	fileContains(const std::string &path, const std::string &expected)
{
	std::ifstream	file(path.c_str());
	std::string		content((std::istreambuf_iterator<char>(file)),
						std::istreambuf_iterator<char>());

	return (content == expected);
}

static void	test_handler_saves_uploaded_file(void)
{
	mkdir("mp_root", 0755);
	mkdir("mp_root/uploads", 0755);
	std::remove("mp_root/uploads/report.txt");

	StaticFileHandler	handler("mp_root");
	handler.setUploadStore("mp_root/uploads");
	HttpRequest			request;
	HttpResponse		response;
	std::string			body =
		"--BOUND\r\n"
		"Content-Disposition: form-data; name=\"file\"; filename=\"report.txt\"\r\n"
		"Content-Type: text/plain\r\n"
		"\r\n"
		"payload-contents"
		"\r\n--BOUND--\r\n";

	request.setMethod("POST");
	request.setUri("/uploads");
	request.setHeaders("Content-Type", "multipart/form-data; boundary=BOUND");
	request.setBody(body);

	bool	handled = handler.handle(request, response);

	TEST(handled, "handle() returns true for a multipart POST");
	TEST(response.getStatusCode() == 201,
		"answers 201 when the uploaded file is created");
	TEST(fileContains("mp_root/uploads/report.txt", "payload-contents"),
		"saves the exact file content under the extracted filename");

	std::remove("mp_root/uploads/report.txt");
	rmdir("mp_root/uploads");
	rmdir("mp_root");
}

static void	test_handler_overwrites_existing_file(void)
{
	mkdir("mp_root2", 0755);
	mkdir("mp_root2/uploads", 0755);
	std::ofstream	existing("mp_root2/uploads/report.txt");
	existing << "old-contents";
	existing.close();

	StaticFileHandler	handler("mp_root2");
	handler.setUploadStore("mp_root2/uploads");
	HttpRequest			request;
	HttpResponse		response;
	std::string			body =
		"--BOUND\r\n"
		"Content-Disposition: form-data; name=\"file\"; filename=\"report.txt\"\r\n"
		"\r\n"
		"new-contents"
		"\r\n--BOUND--\r\n";

	request.setMethod("POST");
	request.setUri("/uploads");
	request.setHeaders("Content-Type", "multipart/form-data; boundary=BOUND");
	request.setBody(body);

	handler.handle(request, response);

	TEST(response.getStatusCode() == 200,
		"answers 200 when an existing file is overwritten");
	TEST(fileContains("mp_root2/uploads/report.txt", "new-contents"),
		"replaces the previous file content");

	std::remove("mp_root2/uploads/report.txt");
	rmdir("mp_root2/uploads");
	rmdir("mp_root2");
}

static void	test_handler_rejects_missing_boundary(void)
{
	StaticFileHandler	handler("mp_root3");
	handler.setUploadStore("mp_root3/uploads");
	HttpRequest			request;
	HttpResponse		response;

	request.setMethod("POST");
	request.setUri("/uploads");
	request.setHeaders("Content-Type", "multipart/form-data");
	request.setBody("irrelevant");

	handler.handle(request, response);

	TEST(response.getStatusCode() == 400,
		"answers 400 when Content-Type has no boundary parameter");
}

static void	test_handler_rejects_body_without_file_part(void)
{
	mkdir("mp_root4", 0755);
	mkdir("mp_root4/uploads", 0755);

	StaticFileHandler	handler("mp_root4");
	handler.setUploadStore("mp_root4/uploads");
	HttpRequest			request;
	HttpResponse		response;
	std::string			body =
		"--BOUND\r\n"
		"Content-Disposition: form-data; name=\"caption\"\r\n"
		"\r\n"
		"no file here"
		"\r\n--BOUND--\r\n";

	request.setMethod("POST");
	request.setUri("/uploads");
	request.setHeaders("Content-Type", "multipart/form-data; boundary=BOUND");
	request.setBody(body);

	handler.handle(request, response);

	TEST(response.getStatusCode() == 400,
		"answers 400 when no part carries a filename");

	rmdir("mp_root4/uploads");
	rmdir("mp_root4");
}

static void	test_handler_strips_path_from_filename(void)
{
	mkdir("mp_root5", 0755);
	mkdir("mp_root5/uploads", 0755);
	std::remove("mp_root5/uploads/escape.txt");
	std::remove("mp_root5/escape.txt");

	StaticFileHandler	handler("mp_root5");
	handler.setUploadStore("mp_root5/uploads");
	HttpRequest			request;
	HttpResponse		response;
	std::string			body =
		"--BOUND\r\n"
		"Content-Disposition: form-data; name=\"file\"; filename=\"../escape.txt\"\r\n"
		"\r\n"
		"contained"
		"\r\n--BOUND--\r\n";

	request.setMethod("POST");
	request.setUri("/uploads");
	request.setHeaders("Content-Type", "multipart/form-data; boundary=BOUND");
	request.setBody(body);

	handler.handle(request, response);

	TEST(response.getStatusCode() == 201,
		"saves a traversal filename inside the upload directory");
	TEST(fileContains("mp_root5/uploads/escape.txt", "contained"),
		"strips the directory component from the filename");

	struct stat	st;
	TEST(stat("mp_root5/escape.txt", &st) != 0,
		"does not write outside the upload directory");

	std::remove("mp_root5/uploads/escape.txt");
	std::remove("mp_root5/escape.txt");
	rmdir("mp_root5/uploads");
	rmdir("mp_root5");
}

static void	test_handler_rejects_dotdot_filename(void)
{
	mkdir("mp_root6", 0755);
	mkdir("mp_root6/uploads", 0755);

	StaticFileHandler	handler("mp_root6");
	handler.setUploadStore("mp_root6/uploads");
	HttpRequest			request;
	HttpResponse		response;
	std::string			body =
		"--BOUND\r\n"
		"Content-Disposition: form-data; name=\"file\"; filename=\"..\"\r\n"
		"\r\n"
		"payload"
		"\r\n--BOUND--\r\n";

	request.setMethod("POST");
	request.setUri("/uploads");
	request.setHeaders("Content-Type", "multipart/form-data; boundary=BOUND");
	request.setBody(body);

	handler.handle(request, response);

	TEST(response.getStatusCode() == 400,
		"rejects a filename that resolves to no base name");

	rmdir("mp_root6/uploads");
	rmdir("mp_root6");
}

static void	test_handler_rejects_oversized_body(void)
{
	mkdir("mp_root7", 0755);
	mkdir("mp_root7/uploads", 0755);

	StaticFileHandler	handler("mp_root7");
	handler.setUploadStore("mp_root7/uploads");
	handler.setMaxBodySize(8);
	HttpRequest			request;
	HttpResponse		response;
	std::string			body =
		"--BOUND\r\n"
		"Content-Disposition: form-data; name=\"file\"; filename=\"big.txt\"\r\n"
		"\r\n"
		"way past the limit"
		"\r\n--BOUND--\r\n";

	request.setMethod("POST");
	request.setUri("/uploads");
	request.setHeaders("Content-Type", "multipart/form-data; boundary=BOUND");
	request.setBody(body);

	handler.handle(request, response);

	TEST(response.getStatusCode() == 413,
		"answers 413 when the body exceeds the maximum size");

	rmdir("mp_root7/uploads");
	rmdir("mp_root7");
}

int	main(void)
{
	test_extract_boundary();
	test_parse_single_file();
	test_parse_mixed_fields();
	test_parse_rejects_missing_boundary();
	test_parse_rejects_truncated_body();
	test_parse_rejects_part_without_disposition();
	test_handler_saves_uploaded_file();
	test_handler_overwrites_existing_file();
	test_handler_rejects_missing_boundary();
	test_handler_rejects_body_without_file_part();
	test_handler_strips_path_from_filename();
	test_handler_rejects_dotdot_filename();
	test_handler_rejects_oversized_body();
	std::cout << std::endl << s_pass << " passed, " << s_fail
		<< " failed" << std::endl;
	return (s_fail == 0 ? 0 : 1);
}

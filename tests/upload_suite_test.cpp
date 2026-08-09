/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   upload_suite_test.cpp                              :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dajesus- <dajesus-@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/30 23:11:19 by dajesus-          #+#    #+#             */
/*   Updated: 2026/08/04 19:37:58 by dajesus-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <iostream>
#include <fstream>
#include <string>
#include <cstdio>
#include <csignal>
#include <sys/stat.h>
#include <sys/resource.h>
#include <unistd.h>
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

/*
 * Reports whether `path` currently exists on the filesystem. Distinguishes a
 * missing file from an existing empty one, which readFile() cannot do because
 * it returns an empty string for both.
 */
static bool	fileExists(const std::string &path)
{
	struct stat	st;

	return (stat(path.c_str(), &st) == 0);
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
	std::remove((root + "/uploads/readonly.txt").c_str());
	std::remove((root + "/uploads/rejected.txt").c_str());
	std::remove((root + "/uploads/field-only.txt").c_str());
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
	TEST(!fileExists("up_root/uploads/rejected.txt"),
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
	std::string		body = "verify on disk";

	request.setMethod("POST");
	request.setUri("/uploads/plain.txt");
	request.setBody(body);

	handler.handle(request, response);

	struct stat	st;
	int			exists = (stat("up_root/uploads/plain.txt", &st) == 0);

	TEST(exists, "uploaded file exists on filesystem");
	TEST(exists && S_ISREG(st.st_mode),
		"uploaded file is a regular file");
	TEST(exists && st.st_size == static_cast<off_t>(body.size()),
		"uploaded file has correct size");

	destroyDirectoryTree("up_root");
}

/* ------------------------------------------------------------------ */
/* Non-POST method does not trigger upload                              */
/* ------------------------------------------------------------------ */

static void	test_get_does_not_upload(void)
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
	if (geteuid() == 0)
	{
		std::cout << "[SKIP] upload to read-only directory (running as root)"
			<< std::endl;
		return;
	}

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

/* ------------------------------------------------------------------ */
/* Malformed upload requests (400)                                      */
/* ------------------------------------------------------------------ */

/*
 * A plain POST whose target URI resolves to an existing directory cannot be
 * written to, so saveFile() must report 400 instead of clobbering the tree.
 */
static void	test_post_to_directory_answers_400(void)
{
	createDirectoryTree("up_root");

	StaticFileHandler	handler("up_root");
	HttpRequest		request;
	HttpResponse	response;

	request.setMethod("POST");
	request.setUri("/uploads/deep");
	request.setBody("into a directory");

	handler.handle(request, response);

	TEST(response.getStatusCode() == 400,
		"POST onto an existing directory answers 400");

	destroyDirectoryTree("up_root");
}

/*
 * multipart/form-data without a boundary parameter is unparseable, so the
 * handler must answer 400 rather than treating the body as a raw upload.
 */
static void	test_multipart_without_boundary_answers_400(void)
{
	createDirectoryTree("up_root");

	StaticFileHandler	handler("up_root");
	HttpRequest		request;
	HttpResponse	response;

	request.setMethod("POST");
	request.setUri("/uploads");
	request.setHeaders("Content-Type", "multipart/form-data");
	request.setBody("--NOPE\r\nContent-Disposition: form-data\r\n\r\nx\r\n--NOPE--\r\n");

	handler.handle(request, response);

	TEST(response.getStatusCode() == 400,
		"multipart without a boundary parameter answers 400");

	destroyDirectoryTree("up_root");
}

/*
 * A multipart body carrying only form fields has no filename to save under,
 * so the handler must answer 400 and leave the upload directory untouched.
 */
static void	test_multipart_without_file_part_answers_400(void)
{
	createDirectoryTree("up_root");

	StaticFileHandler	handler("up_root");
	HttpRequest		request;
	HttpResponse	response;
	std::string		body =
		"--BOUNDARY\r\n"
		"Content-Disposition: form-data; name=\"field\"\r\n"
		"\r\n"
		"just-a-value"
		"\r\n--BOUNDARY--\r\n";

	request.setMethod("POST");
	request.setUri("/uploads");
	request.setHeaders("Content-Type",
		"multipart/form-data; boundary=BOUNDARY");
	request.setBody(body);

	handler.handle(request, response);

	TEST(response.getStatusCode() == 400,
		"multipart with no file part answers 400");
	TEST(!fileExists("up_root/uploads/field-only.txt"),
		"multipart with no file part writes nothing");

	destroyDirectoryTree("up_root");
}

/* ------------------------------------------------------------------ */
/* Containment: symlink escape                          */
/* ------------------------------------------------------------------ */

/*
 * A symlink inside the document root that points outside of it must not let
 * an upload escape containment just because the target file does not exist
 * yet: realpath() fails on the not-yet-created file, but the parent
 * (the symlink itself) resolves outside the root and must be rejected.
 */
static void	test_plain_post_through_symlink_answers_403(void)
{
	createDirectoryTree("up_root");
	mkdir("up_escape", 0755);
	symlink("../../up_escape", "up_root/uploads/link");

	StaticFileHandler	handler("up_root");
	HttpRequest		request;
	HttpResponse	response;

	request.setMethod("POST");
	request.setUri("/uploads/link/pwned.txt");
	request.setBody("escaped the root");

	handler.handle(request, response);

	TEST(response.getStatusCode() == 403,
		"POST through a symlink escaping the root answers 403");
	TEST(!fileExists("up_escape/pwned.txt"),
		"POST through a symlink escaping the root writes nothing outside the root");

	std::remove("up_root/uploads/link");
	std::remove("up_escape/pwned.txt");
	rmdir("up_escape");
	destroyDirectoryTree("up_root");
}

/*
 * Same escape attempt via a multipart upload, which resolves the target
 * path through the same rslv_req_realpath() containment check.
 */
static void	test_multipart_through_symlink_answers_403(void)
{
	createDirectoryTree("up_root");
	mkdir("up_escape", 0755);
	symlink("../../up_escape", "up_root/uploads/link");

	StaticFileHandler	handler("up_root");
	HttpRequest		request;
	HttpResponse	response;
	std::string		body =
		"--BOUNDARY\r\n"
		"Content-Disposition: form-data; name=\"file\"; filename=\"pwned.txt\"\r\n"
		"\r\n"
		"escaped the root"
		"\r\n--BOUNDARY--\r\n";

	request.setMethod("POST");
	request.setUri("/uploads/link");
	request.setHeaders("Content-Type",
		"multipart/form-data; boundary=BOUNDARY");
	request.setBody(body);

	handler.handle(request, response);

	TEST(response.getStatusCode() == 403,
		"multipart upload through a symlink escaping the root answers 403");
	TEST(!fileExists("up_escape/pwned.txt"),
		"multipart upload through a symlink escaping the root writes nothing outside the root");

	std::remove("up_root/uploads/link");
	std::remove("up_escape/pwned.txt");
	rmdir("up_escape");
	destroyDirectoryTree("up_root");
}

/*
 * The escape above walks through a symlinked directory, so the parent of the
 * target resolves outside the root and is caught. A symlink that is itself
 * the target and points at a file that does not exist yet is not: realpath()
 * fails on the dangling link exactly as it does on a new upload, and the
 * parent it falls back to is the real directory holding the link, safely
 * inside the root. open(O_CREAT) then follows the link and creates the file
 * it names, wherever that is, so the dangling link must be refused.
 */
static void	test_plain_post_onto_dangling_symlink_answers_403(void)
{
	createDirectoryTree("up_root");
	mkdir("up_escape", 0755);
	symlink("../../up_escape/pwned.txt", "up_root/uploads/dangling.txt");

	StaticFileHandler	handler("up_root");
	HttpRequest		request;
	HttpResponse	response;

	request.setMethod("POST");
	request.setUri("/uploads/dangling.txt");
	request.setBody("escaped the root");

	handler.handle(request, response);

	TEST(response.getStatusCode() == 403,
		"POST onto a dangling symlink escaping the root answers 403");
	TEST(!fileExists("up_escape/pwned.txt"),
		"POST onto a dangling symlink writes nothing outside the root");

	std::remove("up_root/uploads/dangling.txt");
	std::remove("up_escape/pwned.txt");
	rmdir("up_escape");
	destroyDirectoryTree("up_root");
}

/*
 * Same escape attempt via a multipart upload, whose filename is joined onto
 * the request URI before going through the same containment check.
 */
static void	test_multipart_onto_dangling_symlink_answers_403(void)
{
	createDirectoryTree("up_root");
	mkdir("up_escape", 0755);
	symlink("../../up_escape/pwned.txt", "up_root/uploads/dangling.txt");

	StaticFileHandler	handler("up_root");
	HttpRequest		request;
	HttpResponse	response;
	std::string		body =
		"--BOUNDARY\r\n"
		"Content-Disposition: form-data; name=\"file\"; "
		"filename=\"dangling.txt\"\r\n"
		"\r\n"
		"escaped the root\r\n"
		"--BOUNDARY--\r\n";

	request.setMethod("POST");
	request.setUri("/uploads");
	request.setHeaders("Content-Type",
		"multipart/form-data; boundary=BOUNDARY");
	request.setBody(body);

	handler.handle(request, response);

	TEST(response.getStatusCode() == 403,
		"multipart upload onto a dangling symlink answers 403");
	TEST(!fileExists("up_escape/pwned.txt"),
		"multipart upload onto a dangling symlink writes nothing outside the root");

	std::remove("up_root/uploads/dangling.txt");
	std::remove("up_escape/pwned.txt");
	rmdir("up_escape");
	destroyDirectoryTree("up_root");
}

/*
 * A legitimate upload into a real subdirectory of the root (no symlink
 * involved) must keep answering 201, confirming the containment check on
 * the parent directory does not regress normal uploads.
 */
static void	test_plain_post_legit_new_file_still_201(void)
{
	createDirectoryTree("up_root");

	StaticFileHandler	handler("up_root");
	HttpRequest		request;
	HttpResponse	response;

	request.setMethod("POST");
	request.setUri("/uploads/deep/legit.txt");
	request.setBody("legit content");

	handler.handle(request, response);

	TEST(response.getStatusCode() == 201,
		"legitimate upload inside the root still answers 201");
	TEST(readFile("up_root/uploads/deep/legit.txt") == "legit content",
		"legitimate upload inside the root still writes the correct content");

	std::remove("up_root/uploads/deep/legit.txt");
	destroyDirectoryTree("up_root");
}

/* ------------------------------------------------------------------ */
/* Unwritable target (500)                                              */
/* ------------------------------------------------------------------ */

/*
 * A genuine short/failed write (issue: saveFile() must loop until the whole
 * content is written or the write truly fails) must not leave a truncated
 * file behind. RLIMIT_FSIZE + ignoring SIGXFSZ makes write() fail partway
 * through a large body without killing the process, exercising the same
 * retry-until-done-or-real-failure path a large body or a signal would.
 */
static void	test_post_write_failure_removes_partial_file(void)
{
	createDirectoryTree("up_root");

	struct rlimit	oldLimit;
	struct rlimit	tightLimit;

	getrlimit(RLIMIT_FSIZE, &oldLimit);
	tightLimit.rlim_cur = 16;
	tightLimit.rlim_max = oldLimit.rlim_max;
	setrlimit(RLIMIT_FSIZE, &tightLimit);

	void	(*previousHandler)(int) = signal(SIGXFSZ, SIG_IGN);

	StaticFileHandler	handler("up_root");
	HttpRequest		request;
	HttpResponse	response;
	std::string		content = generateBinaryContent(4096);

	request.setMethod("POST");
	request.setUri("/uploads/toolarge.txt");
	request.setBody(content);

	handler.handle(request, response);

	setrlimit(RLIMIT_FSIZE, &oldLimit);
	signal(SIGXFSZ, previousHandler);

	TEST(response.getStatusCode() == 500,
		"a write that truly fails answers 500");
	TEST(!fileExists("up_root/uploads/toolarge.txt"),
		"a write that truly fails leaves no truncated file behind");

	std::remove("up_root/uploads/toolarge.txt");
	destroyDirectoryTree("up_root");
}

/*
 * Routing an upload through a regular file makes open() fail with ENOTDIR,
 * which is neither a permission nor a missing-directory error, so saveFile()
 * must fall through to 500.
 */
static void	test_post_through_regular_file_answers_500(void)
{
	createDirectoryTree("up_root");

	std::ofstream	blocker("up_root/uploads/plain.txt");
	blocker << "not a directory";
	blocker.close();

	StaticFileHandler	handler("up_root");
	HttpRequest		request;
	HttpResponse	response;

	request.setMethod("POST");
	request.setUri("/uploads/plain.txt/nested.txt");
	request.setBody("unreachable");

	handler.handle(request, response);

	TEST(response.getStatusCode() == 500,
		"POST through a regular file answers 500");
	TEST(readFile("up_root/uploads/plain.txt") == "not a directory",
		"failed upload leaves the blocking file untouched");

	destroyDirectoryTree("up_root");
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
	test_get_does_not_upload();
	test_upload_to_missing_directory();
	test_upload_to_readonly_parent();
	test_plain_post_through_symlink_answers_403();
	test_multipart_through_symlink_answers_403();
	test_plain_post_onto_dangling_symlink_answers_403();
	test_multipart_onto_dangling_symlink_answers_403();
	test_plain_post_legit_new_file_still_201();
	test_post_to_directory_answers_400();
	test_multipart_without_boundary_answers_400();
	test_multipart_without_file_part_answers_400();
	test_post_through_regular_file_answers_500();
	test_post_write_failure_removes_partial_file();
	std::cout << std::endl << s_pass << " passed, " << s_fail
		<< " failed" << std::endl;
	return (s_fail == 0 ? 0 : 1);
}

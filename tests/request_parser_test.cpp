/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   request_parser_test.cpp                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: galves-a <galves-a@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/08/01 10:00:00 by galves-a          #+#    #+#             */
/*   Updated: 2026/08/01 10:00:00 by galves-a         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <iostream>
#include <sstream>
#include <string>

#include "http/RequestParser.hpp"

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

/*
 * Renders `value` as a decimal string. Replaces std::to_string, which is not
 * available under -std=c++98.
 */
static std::string	toDecimal(std::size_t value)
{
	std::ostringstream	out;

	out << value;
	return (out.str());
}

/*
 * Builds a byte string cycling through all 256 octet values so the body
 * carries ':', '\r' and '\n' bytes that must never be read as headers.
 */
static std::string	allByteValues(std::size_t size)
{
	std::string	result;

	result.reserve(size);
	for (std::size_t i = 0; i < size; ++i)
		result.push_back(static_cast<char>(i % 256));
	return (result);
}

/*
 * Feeds `raw` to `parser` in slices of `chunk` bytes, reproducing the partial
 * reads a socket delivers when a request spans several recv() calls.
 */
static void	feedInChunks(RequestParser &parser, const std::string &raw,
		std::size_t chunk)
{
	for (std::size_t off = 0; off < raw.size(); off += chunk)
	{
		std::size_t	n = raw.size() - off;

		if (n > chunk)
			n = chunk;
		parser.feed(raw.data() + off, static_cast<ssize_t>(n));
	}
}

/*
 * Assembles a POST request whose headers declare `body`'s exact length.
 */
static std::string	buildPost(const std::string &uri, const std::string &body)
{
	return ("POST " + uri + " HTTP/1.1\r\nHost: x\r\nContent-Length: "
		+ toDecimal(body.size()) + "\r\n\r\n" + body);
}

/* ------------------------------------------------------------------ */
/* Body bytes must not be parsed as headers                             */
/* ------------------------------------------------------------------ */

/*
 * A binary body contains ':' bytes. The header parser must stop at the first
 * "\r\n\r\n" instead of consuming the body while looking for more headers.
 */
static void	test_binary_body_is_not_scanned_as_headers(void)
{
	std::string		body = allByteValues(256);
	std::string		raw = buildPost("/uploads/binary.bin", body);
	RequestParser	parser;

	parser.feed(raw.data(), static_cast<ssize_t>(raw.size()));

	TEST(parser.get_psr_state() == COMPLETE,
		"binary body with ':' bytes parses to COMPLETE");
	TEST(parser.get_error_code() == 0,
		"binary body with ':' bytes reports no error");
	TEST(parser.getRequest().getBody() == body,
		"binary body is preserved byte for byte");
}

/*
 * A text body that looks like a header block ("a:b\r\n...") must be kept in
 * the body, not folded into the request's headers.
 */
static void	test_header_shaped_body_stays_in_body(void)
{
	std::string		body = "a:b\r\nHost: evil\r\nc:d";
	std::string		raw = buildPost("/uploads/notes.txt", body);
	RequestParser	parser;

	parser.feed(raw.data(), static_cast<ssize_t>(raw.size()));

	TEST(parser.get_psr_state() == COMPLETE,
		"header-shaped body parses to COMPLETE");
	TEST(parser.getRequest().getBody() == body,
		"header-shaped body is preserved verbatim");
	TEST(parser.getRequest().getHeaderValue("Host") == "x",
		"body headers do not override the real Host header");
}

/* ------------------------------------------------------------------ */
/* Body size must not be measured against MAX_HEADER_SIZE               */
/* ------------------------------------------------------------------ */

/*
 * A body larger than MAX_HEADER_SIZE is legal. Only the header block is
 * subject to the 431 limit, so a big upload must still parse cleanly.
 */
static void	test_body_larger_than_max_header_size(void)
{
	std::string		body(MAX_HEADER_SIZE + 808, 'B');
	std::string		raw = buildPost("/uploads/big.bin", body);
	RequestParser	parser;

	parser.feed(raw.data(), static_cast<ssize_t>(raw.size()));

	TEST(parser.get_error_code() != 431,
		"a body over MAX_HEADER_SIZE is not rejected with 431");
	TEST(parser.get_psr_state() == COMPLETE,
		"a body over MAX_HEADER_SIZE parses to COMPLETE");
	TEST(parser.getRequest().getBody().size() == body.size(),
		"a body over MAX_HEADER_SIZE arrives complete");
}

/*
 * The 431 limit still has to fire when the *header block* itself is oversized.
 */
static void	test_oversized_header_block_still_rejected(void)
{
	std::string		raw = "POST /u HTTP/1.1\r\nHost: x\r\n";
	RequestParser	parser;

	while (raw.size() < MAX_HEADER_SIZE * 2)
		raw += "X-Padding: " + std::string(60, 'v') + "\r\n";
	raw += "Content-Length: 3\r\n\r\nabc";

	parser.feed(raw.data(), static_cast<ssize_t>(raw.size()));

	TEST(parser.get_psr_state() == ERROR,
		"an oversized header block is rejected");
	TEST(parser.get_error_code() == 431,
		"an oversized header block answers 431");
}

/* ------------------------------------------------------------------ */
/* Keep-alive: pipelined requests                                       */
/* ------------------------------------------------------------------ */

/*
 * Two requests may arrive in a single read. The first must consume only its
 * own body, leaving the second intact for reset() to parse.
 */
static void	test_pipelined_requests(void)
{
	std::string		body = "HELLOWORLD!!";
	std::string		raw = buildPost("/first.txt", body)
		+ "GET /second HTTP/1.1\r\nHost: x\r\n\r\n";
	RequestParser	parser;

	parser.feed(raw.data(), static_cast<ssize_t>(raw.size()));

	TEST(parser.get_psr_state() == COMPLETE,
		"first pipelined request parses to COMPLETE");
	TEST(parser.getRequest().getUri() == "/first.txt",
		"first pipelined request keeps its URI");
	TEST(parser.getRequest().getBody() == body,
		"first pipelined request keeps its body");

	parser.reset();

	TEST(parser.get_psr_state() == COMPLETE,
		"second pipelined request parses to COMPLETE");
	TEST(parser.get_error_code() == 0,
		"second pipelined request reports no error");
	TEST(parser.getRequest().getUri() == "/second",
		"second pipelined request keeps its URI");
}

/* ------------------------------------------------------------------ */
/* Regression guards for the surrounding parser paths                   */
/* ------------------------------------------------------------------ */

/*
 * Splitting the same request across recv() boundaries must not change the
 * result, including the pathological one-byte-at-a-time case.
 */
static void	test_split_feeds_are_equivalent(void)
{
	std::string	body = allByteValues(300);
	std::string	raw = buildPost("/uploads/split.bin", body);
	std::size_t	sizes[3] = {1, 7, 4096};

	for (std::size_t i = 0; i < 3; ++i)
	{
		RequestParser	parser;

		feedInChunks(parser, raw, sizes[i]);

		TEST(parser.get_psr_state() == COMPLETE
			&& parser.getRequest().getBody() == body,
			"split feed reassembles the request (chunk "
			+ toDecimal(sizes[i]) + ")");
	}
}

/*
 * Chunked transfer-encoding is decoded through a different branch and must
 * keep working after the header/body split.
 */
static void	test_chunked_body_still_parses(void)
{
	std::string		raw =
		"POST /c HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\n\r\n"
		"5\r\nHELLO\r\n0\r\n\r\n";
	RequestParser	parser;

	parser.feed(raw.data(), static_cast<ssize_t>(raw.size()));

	TEST(parser.get_psr_state() == COMPLETE,
		"chunked request parses to COMPLETE");
	TEST(parser.getRequest().getBody() == "HELLO",
		"chunked request decodes its body");
}

/*
 * A request carrying both Transfer-Encoding and Content-Length is ambiguous
 * and must be rejected with 400.
 */
static void	test_conflicting_length_headers_rejected(void)
{
	std::string		raw =
		"POST /c HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\n"
		"Content-Length: 5\r\n\r\nHELLO";
	RequestParser	parser;

	parser.feed(raw.data(), static_cast<ssize_t>(raw.size()));

	TEST(parser.get_psr_state() == ERROR,
		"Transfer-Encoding plus Content-Length is rejected");
	TEST(parser.get_error_code() == 400,
		"Transfer-Encoding plus Content-Length answers 400");
}

/*
 * A request without a Host header is invalid under HTTP/1.1.
 */
static void	test_missing_host_header_rejected(void)
{
	std::string		raw = "GET / HTTP/1.1\r\nAccept: */*\r\n\r\n";
	RequestParser	parser;

	parser.feed(raw.data(), static_cast<ssize_t>(raw.size()));

	TEST(parser.get_psr_state() == ERROR,
		"a request without Host is rejected");
}

/*
 * A plain GET with several headers must still parse, with every header value
 * reaching the request intact.
 */
static void	test_simple_get_with_headers(void)
{
	std::string		raw =
		"GET /index.html HTTP/1.1\r\nHost: example.com\r\n"
		"Accept: */*\r\nUser-Agent: probe/1.0\r\n\r\n";
	RequestParser	parser;

	parser.feed(raw.data(), static_cast<ssize_t>(raw.size()));

	TEST(parser.get_psr_state() == COMPLETE,
		"a plain GET parses to COMPLETE");
	TEST(parser.getRequest().getUri() == "/index.html",
		"a plain GET keeps its URI");
	TEST(parser.getRequest().getHeaderValue("Host") == "example.com",
		"a plain GET keeps its Host header");
	TEST(parser.getRequest().getHeaderValue("User-Agent") == "probe/1.0",
		"a plain GET keeps its last header");
}

/* ------------------------------------------------------------------ */
/* Buffer growth must be bounded (memory exhaustion)                    */
/* ------------------------------------------------------------------ */

/*
 * A Content-Length far larger than anything the server accepts must be
 * refused as soon as the headers are read, while the body is still unsent.
 */
static void	test_absurd_content_length_rejected(void)
{
	std::string		raw = "POST /u HTTP/1.1\r\nHost: x\r\n"
		"Content-Length: 99999999999999\r\n\r\n";
	RequestParser	parser;

	parser.setMaxBodySize(256);
	parser.feed(raw.data(), static_cast<ssize_t>(raw.size()));

	TEST(parser.get_psr_state() == ERROR,
		"an absurd Content-Length is rejected");
	TEST(parser.get_error_code() == 413,
		"an absurd Content-Length answers 413");
}

/*
 * A Content-Length of more digits than a size_t can hold must not wrap into a
 * small number that the parser would then honour.
 */
static void	test_content_length_overflow_rejected(void)
{
	std::string		raw = "POST /u HTTP/1.1\r\nHost: x\r\n"
		"Content-Length: " + std::string(40, '9') + "\r\n\r\n";
	RequestParser	parser;

	parser.setMaxBodySize(-1);
	parser.feed(raw.data(), static_cast<ssize_t>(raw.size()));

	TEST(parser.get_psr_state() == ERROR,
		"a Content-Length that overflows size_t is rejected");
	TEST(parser.get_error_code() == 413,
		"a Content-Length that overflows size_t answers 413");
}

/*
 * "Content-Length: -1" is not a decimal number. strtoul used to wrap it to a
 * huge value; it is malformed and must answer 400, not be buffered.
 */
static void	test_negative_content_length_rejected(void)
{
	std::string		raw = "POST /u HTTP/1.1\r\nHost: x\r\n"
		"Content-Length: -1\r\n\r\n";
	RequestParser	parser;

	parser.setMaxBodySize(256);
	parser.feed(raw.data(), static_cast<ssize_t>(raw.size()));

	TEST(parser.get_psr_state() == ERROR,
		"a negative Content-Length is rejected");
	TEST(parser.get_error_code() == 400,
		"a negative Content-Length answers 400");
}

/*
 * A Content-Length that is representable but over the configured limit must
 * still be refused before the body is buffered.
 */
static void	test_content_length_over_limit_rejected(void)
{
	std::string		raw = "POST /u HTTP/1.1\r\nHost: x\r\n"
		"Content-Length: 4096\r\n\r\n";
	RequestParser	parser;

	parser.setMaxBodySize(256);
	parser.feed(raw.data(), static_cast<ssize_t>(raw.size()));

	TEST(parser.get_psr_state() == ERROR,
		"a Content-Length over the limit is rejected");
	TEST(parser.get_error_code() == 413,
		"a Content-Length over the limit answers 413");
}

/*
 * A body within the configured limit must keep parsing normally, so the guard
 * does not turn every upload into a 413.
 */
static void	test_content_length_within_limit_accepted(void)
{
	std::string		body(200, 'B');
	std::string		raw = buildPost("/u", body);
	RequestParser	parser;

	parser.setMaxBodySize(256);
	parser.feed(raw.data(), static_cast<ssize_t>(raw.size()));

	TEST(parser.get_psr_state() == COMPLETE,
		"a body within the limit parses to COMPLETE");
	TEST(parser.getRequest().getBody() == body,
		"a body within the limit arrives complete");
}

/*
 * A header block that never gets its "\r\n\r\n" must be cut off once it passes
 * MAX_HEADER_SIZE instead of buffering whatever the client keeps sending.
 */
static void	test_unterminated_header_block_rejected(void)
{
	std::string		raw = "POST /u HTTP/1.1\r\nHost: x\r\n";
	RequestParser	parser;

	while (raw.size() <= MAX_REQUEST_LINE_SIZE + MAX_HEADER_SIZE)
		raw += "X-Padding: " + std::string(60, 'v') + "\r\n";

	feedInChunks(parser, raw, 512);

	TEST(parser.get_psr_state() == ERROR,
		"an unterminated header block is rejected");
	TEST(parser.get_error_code() == 431,
		"an unterminated header block answers 431");
}

/*
 * A header value may be padded with spaces and tabs. prs_headers only strips
 * one leading space, so the length check has to tolerate the rest instead of
 * calling a legal request malformed.
 */
static void	test_padded_content_length_accepted(void)
{
	std::string	bodies[3] = {"5 ", " 5", "\t5"};

	for (std::size_t i = 0; i < 3; ++i)
	{
		std::string		raw = "POST /u HTTP/1.1\r\nHost: x\r\nContent-Length: "
			+ bodies[i] + "\r\n\r\nHELLO";
		RequestParser	parser;

		parser.feed(raw.data(), static_cast<ssize_t>(raw.size()));

		TEST(parser.get_psr_state() == COMPLETE
			&& parser.getRequest().getBody() == "HELLO",
			"a padded Content-Length is read as its number (case "
			+ toDecimal(i) + ")");
	}
}

/*
 * A header block just under MAX_HEADER_SIZE is legal however it arrives. The
 * buffer still holds the request line while the block is being read, so a cap
 * that counted it would reject the request only on the split path.
 */
static void	test_large_legal_header_block_survives_splitting(void)
{
	std::string	line = "GET /" + std::string(MAX_URI_LENGTH - 48, 'u')
		+ " HTTP/1.1\r\n";
	std::string	raw = line + "Host: x\r\n";
	std::size_t	sizes[2] = {512, 65536};

	while (raw.size() - line.size() < MAX_HEADER_SIZE - 100)
		raw += "X-Pad: " + std::string(60, 'v') + "\r\n";
	raw += "\r\n";
	for (std::size_t i = 0; i < 2; ++i)
	{
		RequestParser	parser;

		feedInChunks(parser, raw, sizes[i]);

		TEST(parser.get_psr_state() == COMPLETE,
			"a large legal header block parses to COMPLETE (chunk "
			+ toDecimal(sizes[i]) + ")");
	}
}

/*
 * A header block still under MAX_HEADER_SIZE is merely incomplete: the parser
 * must keep waiting for the rest instead of answering 431.
 */
static void	test_incomplete_header_block_keeps_waiting(void)
{
	std::string		raw = "POST /u HTTP/1.1\r\nHost: x\r\nContent-Length: 3\r\n";
	RequestParser	parser;

	parser.feed(raw.data(), static_cast<ssize_t>(raw.size()));

	TEST(parser.get_psr_state() != ERROR,
		"an incomplete header block is not rejected early");

	std::string	rest = "\r\nabc";

	parser.feed(rest.data(), static_cast<ssize_t>(rest.size()));

	TEST(parser.get_psr_state() == COMPLETE,
		"an incomplete header block completes when the rest arrives");
}

/*
 * A chunk announcing more bytes than the limit allows must be refused when its
 * size line is read, before the parser waits for those bytes.
 */
static void	test_oversized_chunk_rejected(void)
{
	std::string		raw = "POST /c HTTP/1.1\r\nHost: x\r\n"
		"Transfer-Encoding: chunked\r\n\r\n3e8\r\n";
	RequestParser	parser;

	parser.setMaxBodySize(64);
	parser.feed(raw.data(), static_cast<ssize_t>(raw.size()));

	TEST(parser.get_psr_state() == ERROR,
		"a chunk larger than the limit is rejected");
	TEST(parser.get_error_code() == 413,
		"a chunk larger than the limit answers 413");
}

/*
 * Chunks that each fit the limit must not add up past it either.
 */
static void	test_chunked_total_over_limit_rejected(void)
{
	std::string		raw = "POST /c HTTP/1.1\r\nHost: x\r\n"
		"Transfer-Encoding: chunked\r\n\r\n";
	RequestParser	parser;

	parser.setMaxBodySize(64);
	for (std::size_t i = 0; i < 4; ++i)
		raw += "20\r\n" + std::string(32, 'C') + "\r\n";

	parser.feed(raw.data(), static_cast<ssize_t>(raw.size()));

	TEST(parser.get_psr_state() == ERROR,
		"chunks adding up past the limit are rejected");
	TEST(parser.get_error_code() == 413,
		"chunks adding up past the limit answer 413");
}

/*
 * A chunk size line that never gets its "\r\n" leaves the parser waiting, so
 * the buffer ceiling is what has to stop the client from filling memory.
 */
static void	test_unterminated_chunk_size_line_rejected(void)
{
	std::string		raw = "POST /c HTTP/1.1\r\nHost: x\r\n"
		"Transfer-Encoding: chunked\r\n\r\n";
	RequestParser	parser;

	parser.setMaxBodySize(64);
	parser.feed(raw.data(), static_cast<ssize_t>(raw.size()));

	std::string	flood(MAX_HEADER_SIZE + 512, 'a');

	feedInChunks(parser, flood, 512);

	TEST(parser.get_psr_state() == ERROR,
		"an unterminated chunk size line is cut off");
	TEST(parser.get_error_code() == 413,
		"an unterminated chunk size line answers 413");
}

/*
 * The limit travels with the parser through a copy, since every connection is
 * stored by assigning it into the client map.
 */
static void	test_limit_survives_assignment(void)
{
	std::string		raw = "POST /u HTTP/1.1\r\nHost: x\r\n"
		"Content-Length: 4096\r\n\r\n";
	RequestParser	source;
	RequestParser	parser;

	source.setMaxBodySize(256);
	parser = source;
	parser.feed(raw.data(), static_cast<ssize_t>(raw.size()));

	TEST(parser.get_error_code() == 413,
		"the body limit survives parser assignment");
}

int	main(void)
{
	test_binary_body_is_not_scanned_as_headers();
	test_header_shaped_body_stays_in_body();
	test_body_larger_than_max_header_size();
	test_oversized_header_block_still_rejected();
	test_pipelined_requests();
	test_split_feeds_are_equivalent();
	test_chunked_body_still_parses();
	test_conflicting_length_headers_rejected();
	test_missing_host_header_rejected();
	test_simple_get_with_headers();
	test_absurd_content_length_rejected();
	test_content_length_overflow_rejected();
	test_negative_content_length_rejected();
	test_content_length_over_limit_rejected();
	test_content_length_within_limit_accepted();
	test_unterminated_header_block_rejected();
	test_padded_content_length_accepted();
	test_large_legal_header_block_survives_splitting();
	test_incomplete_header_block_keeps_waiting();
	test_oversized_chunk_rejected();
	test_chunked_total_over_limit_rejected();
	test_unterminated_chunk_size_line_rejected();
	test_limit_survives_assignment();
	std::cout << std::endl << s_pass << " passed, " << s_fail
		<< " failed" << std::endl;
	return (s_fail == 0 ? 0 : 1);
}

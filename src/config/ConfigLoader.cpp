/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ConfigLoader.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/18 11:25:05 by dajesus-          #+#    #+#             */
/*   Updated: 2026/08/01 15:37:53 by jucoelho         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "config/ConfigLoader.hpp"
#include "utils/Logger.hpp"
#include "utils/Utils.hpp"
#include <cerrno>
#include <climits>
#include <stdexcept>

/**
 * @brief Checks that a string is a non-empty sequence of decimal digits.
 * @param token The string to inspect.
 * @return true when every character is a digit and the string is not empty.
 */
static bool	isAllDigits(const std::string &token)
{
	if (token.empty())
		return (false);
	return (token.find_first_not_of("0123456789") == std::string::npos);
}

/**
 * @brief Checks that a string is a dotted-quad IPv4 literal (e.g. "127.0.0.1").
 * Each of the four octets must be numeric and within 0-255. This mirrors what
 * inet_pton() accepts in Socket::bind(), so an accepted host never fails later.
 * @param host The host string to validate.
 * @return true when the string is a valid IPv4 literal.
 */
static bool	isIpv4Literal(const std::string &host)
{
	size_t	start = 0;
	int		octets = 0;

	while (octets < 4)
	{
		size_t		dot = host.find('.', start);
		std::string	part = (dot == std::string::npos)
			? host.substr(start) : host.substr(start, dot - start);

		if (!isAllDigits(part) || part.size() > 3)
			return (false);

		std::istringstream	iss(part);
		int					value = 0;
		iss >> value;
		if (value < 0 || value > 255)
			return (false);

		octets++;
		if (dot == std::string::npos)
			return (octets == 4);
		start = dot + 1;
	}
	return (false);
}

ConfigLoader::ConfigLoader(void) : _file_path("conf/default.conf")
{
}

ConfigLoader::ConfigLoader(const std::string &file_path)
	: _file_path(file_path)
{
}

ConfigLoader::ConfigLoader(const ConfigLoader &copy)
{
	this->_file_path = copy._file_path;
	this->_servers = copy._servers;
	this->_tree = copy._tree;
}

ConfigLoader &ConfigLoader::operator=(const ConfigLoader &other)
{
	if (this != &other)
	{
		_file_path = other._file_path;
		_servers = other._servers;
		_tree = other._tree;
	}
	return (*this);
}

ConfigLoader::~ConfigLoader(void)
{
}

std::string ConfigLoader::configPath(void)
{
	std::ifstream file(_file_path.c_str());

	if (!file.is_open())
		throw std::runtime_error("Cannot open config file: " + _file_path);

	std::stringstream buffer;
	buffer << file.rdbuf();
	return (buffer.str());
}

/**
 * @brief Loads the configuration file and builds one ServerConfig per server
 * block. Reads the raw file, tokenizes it, builds the AST and extracts the
 * directives and location blocks of every server found in it.
 * @return The populated ServerConfig list, in declaration order.
 * @throw std::runtime_error when the file cannot be read or is invalid.
 */
std::vector<ServerConfig> ConfigLoader::loader(void)
{
	Lexer				lexer(configPath());
	std::vector<Token>	tokens = lexer.tokenize();
	ConfigParser		parser(tokens);

	_tree = parser.parse();
	_servers.clear();

	std::vector<ConfigBlock>::const_iterator	block_it;
	for (block_it = _tree.children.begin();
	     block_it != _tree.children.end(); ++block_it)
	{
		if (block_it->name != "server")
			continue;

		ServerConfig	server;

		parse_directives(*block_it, server);
		parse_locations(*block_it, server);
		if (server.root.empty())
			server.root = DEFAULT_ROOT;
		_servers.push_back(server);
	}
	if (_servers.empty())
		throw std::runtime_error("config: no server block found");
	return (_servers);
}

/**
 * @brief Validates and converts a port token into an integer.
 * Rejects empty tokens, any non-digit character (so "8080abc" and "80.5" are
 * refused instead of being silently truncated) and values outside 1-65535.
 * @param token The raw port string taken from the listen directive.
 * @return The port as an int.
 * @throw std::runtime_error when the token is not a valid port.
 */
int	ConfigLoader::parsePort(const std::string &token)
{
	if (token.empty())
		throw std::runtime_error("listen: missing port");
	if (!isAllDigits(token))
		throw std::runtime_error("listen: invalid port '" + token + "'");

	std::istringstream	iss(token);
	long				value = 0;

	iss >> value;
	if (iss.fail() || value < 1 || value > 65535)
		throw std::runtime_error("listen: port out of range (1-65535): '"
			+ token + "'");
	return (static_cast<int>(value));
}

/**
 * @brief Validates a host token and normalises it into an IPv4 literal.
 * "localhost" is mapped to 127.0.0.1; any other name is refused because
 * Socket::bind() resolves hosts with inet_pton(), which only accepts literals.
 * @param token The raw host string taken from the listen directive.
 * @return The host as a dotted-quad IPv4 literal.
 * @throw std::runtime_error when the host is empty or cannot be resolved.
 */
std::string	ConfigLoader::parseHost(const std::string &token)
{
	if (token.empty())
		throw std::runtime_error("listen: missing host before ':'");
	if (token == "localhost")
		return ("127.0.0.1");
	if (!isIpv4Literal(token))
		throw std::runtime_error("listen: invalid host '" + token
			+ "' (expected an IPv4 address or 'localhost')");
	return (token);
}

/**
 * @brief Applies the arguments of a single listen directive to a server.
 * Accepts either "port" or "host:port", and rejects extra arguments as well as
 * any token carrying more than one colon.
 * @param args The argument list of the listen directive.
 * @param server The server block being filled.
 * @throw std::runtime_error when the directive is malformed.
 */
void	ConfigLoader::applyListen(const std::vector<std::string> &args,
			ServerConfig &server)
{
	if (args.empty())
		throw std::runtime_error("listen directive requires an argument");
	if (args.size() > 1)
		throw std::runtime_error("listen: expects a single host:port argument");

	const std::string	&value = args[0];
	size_t				colon = value.find(':');

	if (colon == std::string::npos)
	{
		server.port = parsePort(value);
		return ;
	}
	if (value.find(':', colon + 1) != std::string::npos)
		throw std::runtime_error("listen: malformed host:port '" + value + "'");
	server.host = parseHost(value.substr(0, colon));
	server.port = parsePort(value.substr(colon + 1));
}

/**
 * @brief Collects every name declared by a server_name directive.
 * Names are stored lowercased because the host of a request is case-insensitive,
 * so the Host header can be compared against them directly at request time.
 * @param directive The server_name directive taken from the AST.
 * @param server The server block being filled.
 * @throw std::runtime_error when the directive carries no name.
 */
void	ConfigLoader::parse_names(const ConfigDirective &directive,
			ServerConfig &server)
{
	if (directive.args.empty())
		throw std::runtime_error("server_name directive requires a name");
	for (size_t i = 0; i < directive.args.size(); i++)
		server.serverNames.push_back(toLower(directive.args[i]));
}

/**
 * @brief Validates a root directive and normalises the path it carries.
 * Trailing slashes are stripped so "www/" and "www" resolve identically once
 * the URI segments are appended to them.
 * @param d The root directive taken from the AST.
 * @return The document root path.
 * @throw std::runtime_error when the directive does not carry exactly one path.
 */
std::string	ConfigLoader::parseRoot(const ConfigDirective &d)
{
	if (d.args.size() != 1)
		throw std::runtime_error("root: expects a single path argument");
	if (d.args[0].empty())
		throw std::runtime_error("root: path cannot be empty");

	std::string	path = d.args[0];

	while (path.size() > 1 && path[path.size() - 1] == '/')
		path.erase(path.size() - 1);
	return (path);
}

/**
 * @brief Applies every directive declared directly inside a server block.
 * Only one listening socket and one index are supported per server today, so a
 * warning is emitted when several listen directives are declared, when none is
 * present, or when several index directives are declared.
 * @param block The server block taken from the AST.
 * @param server The server block being filled.
 * @throw std::runtime_error when a directive is malformed.
 */
void	ConfigLoader::parse_directives(const ConfigBlock &block,
			ServerConfig &server)
{
	std::vector<ConfigDirective>::const_iterator	it;
	int												found = 0;
	int												indexes = 0;

	for (it = block.directives.begin(); it != block.directives.end(); ++it)
	{
		if (it->name == "listen")
		{
			applyListen(it->args, server);
			found++;
		}
		else if (it->name == "server_name")
			parse_names(*it, server);
		else if (it->name == "root")
			server.root = parseRoot(*it);
		else if (it->name == "index")
		{
			parseIndex(server.index, *it);
			indexes++;
		}
		else if (it->name == "autoindex")
			parseAutoindex(server.autoindex, *it);
		else if (it->name == "client_max_body_size")
			server.clientMaxBodySize = parseBodySize(*it);
		else if (it->name == "error_page")
			parseErrorPage(server.errorPages, *it);
	}

	if (indexes > 1)
	{
		std::ostringstream	idx;

		idx << indexes << " index directives found, only the last one is used ("
			<< server.index << ")";
		Logger::warning(idx.str());
	}

	std::ostringstream	oss;
	if (found == 0)
	{
		oss << "no listen directive found, using default "
			<< server.host << ":" << server.port;
		Logger::warning(oss.str());
		return ;
	}
	if (found > 1)
	{
		oss << found << " listen directives found, only the last one is used ("
			<< server.host << ":" << server.port << ")";
		Logger::warning(oss.str());
	}
}

/**
 * @brief Applies every location block declared inside a server block.
 * A location without its own root inherits the one of the server, which is
 * resolved at request time by the Router. A location without its own index
 * or autoindex inherits the one of the server it was declared in. A location
 * without its own client_max_body_size keeps the -1 sentinel, so the Router
 * falls back to the server value at request time.
 * @param block The server block taken from the AST.
 * @param server The server block being filled.
 * @throw std::runtime_error when a location block is malformed.
 */
void	ConfigLoader::parse_locations(const ConfigBlock &block,
			ServerConfig &server)
{
	std::vector<ConfigBlock>::const_iterator	it;

	for (it = block.children.begin(); it != block.children.end(); ++it)
	{
		if (it->name != "location")
			continue;
		if (it->args.empty())
			throw std::runtime_error("location block requires a path argument");

		LocationConfig	loc(it->args[0]);

		loc.autoindex = server.autoindex;

		std::vector<ConfigDirective>::const_iterator	dit;
		for (dit = it->directives.begin(); dit != it->directives.end(); ++dit)
		{
			if (dit->name == "autoindex")
				parseAutoindex(loc.autoindex, *dit);
			else if (dit->name == "root")
				loc.root = parseRoot(*dit);
			else if (dit->name == "index")
				parseIndex(loc.index, *dit);
			else if (dit->name == "client_max_body_size")
				loc.clientMaxBodySize = parseBodySize(*dit);
		}

		if (loc.index.empty())
			loc.index = server.index;
		server.locations.push_back(loc);
	}
}

/**
 * @brief Converts an "on"/"off" token into a boolean.
 * @param s The token to convert.
 * @return true for "on", false for "off".
 * @throw std::runtime_error when the token is neither.
 */
bool	ConfigLoader::parseBool(const std::string &s)
{
	if (s == "on")
		return (true);
	if (s == "off")
		return (false);
	throw std::runtime_error("expected 'on' or 'off', got: '" + s + "'");
}

/**
 * @brief Applies an autoindex directive to a server or a location.
 * @param autoindex The destination holding the server or location flag.
 * @param d The autoindex directive taken from the AST.
 * @throw std::runtime_error when the directive is malformed.
 */
void	ConfigLoader::parseAutoindex(bool &autoindex,
			const ConfigDirective &d)
{
	if (d.args.size() != 1)
		throw std::runtime_error("'autoindex' expects 'on' or 'off'");
	autoindex = parseBool(d.args[0]);
}

/**
 * @brief Validates an index directive and stores its file name.
 * Only one file name is supported, so a list of fallbacks is refused instead
 * of being silently truncated to its first entry.
 * @param index The destination holding the server or location index.
 * @param d The index directive taken from the AST.
 * @throw std::runtime_error when the directive carries anything other than a
 * single non-empty file name.
 */
void ConfigLoader::parseIndex(std::string &index, const ConfigDirective &d)
{
	if (d.args.size() != 1)
		throw std::runtime_error(
		    "'index' expects a single file name");
	if (d.args[0].empty())
		throw std::runtime_error(
		    "'index' expects a non-empty file name");
	index = d.args[0];
}

/**
 * @brief Converts a client_max_body_size directive into a byte count.
 * The value is a number of bytes, optionally suffixed by k, m or g for
 * kibibytes, mebibytes or gibibytes, mirroring the units NGINX accepts.
 * @param d The client_max_body_size directive taken from the AST.
 * @return The maximum body size in bytes.
 * @throw std::runtime_error when the directive carries anything other than a
 * single size that fits in a long.
 */
long ConfigLoader::parseBodySize(const ConfigDirective &d)
{
	if (d.args.size() != 1)
		throw std::runtime_error(
		    "'client_max_body_size' expects a single size argument");

	std::string	digits = d.args[0];
	long		multiplier = 1;

	if (!digits.empty())
	{
		char	suffix = digits[digits.size() - 1];

		if (suffix == 'k' || suffix == 'K')
			multiplier = 1024L;
		else if (suffix == 'm' || suffix == 'M')
			multiplier = 1024L * 1024L;
		else if (suffix == 'g' || suffix == 'G')
			multiplier = 1024L * 1024L * 1024L;
		if (multiplier != 1)
			digits.erase(digits.size() - 1);
	}
	if (!isAllDigits(digits))
		throw std::runtime_error(
		    "'client_max_body_size' expects a size in bytes, optionally "
		    "suffixed by k, m or g");

	errno = 0;

	long	value = std::strtol(digits.c_str(), NULL, 10);

	if (errno == ERANGE || value > LONG_MAX / multiplier)
		throw std::runtime_error(
		    "'client_max_body_size' value is too large: '" + d.args[0] + "'");
	return (value * multiplier);
}

/**
 * @brief Validates a single error_page status code and converts it to an int.
 * Only client and server error codes are accepted, so a typo such as "200" or
 * "4o4" is refused at load time instead of silently never matching a response.
 * @param token The raw status code taken from the error_page directive.
 * @return The status code as an int.
 * @throw std::runtime_error when the token is not a code within 400-599.
 */
int	ConfigLoader::parseErrorCode(const std::string &token)
{
	if (!isAllDigits(token) || token.size() != 3)
		throw std::runtime_error("error_page: invalid status code '"
			+ token + "'");

	std::istringstream	iss(token);
	int					code = 0;

	iss >> code;
	if (iss.fail() || code < 400 || code > 599)
		throw std::runtime_error("error_page: status code out of range "
			"(400-599): '" + token + "'");
	return (code);
}

/**
 * @brief Applies an error_page directive to the map of a server block.
 * Every argument but the last is a status code, so a single directive can map
 * several codes to the same page. A code declared twice keeps the last page,
 * which mirrors how the other directives resolve duplicates.
 * @param pages The destination holding the error pages of the server.
 * @param d The error_page directive taken from the AST.
 * @throw std::runtime_error when a code or the page path is malformed.
 */
void	ConfigLoader::parseErrorPage(std::map<int, std::string> &pages,
			const ConfigDirective &d)
{
	if (d.args.size() < 2)
		throw std::runtime_error(
			"'error_page' expects at least one status code and a path");

	const std::string	&path = d.args[d.args.size() - 1];

	if (path.empty())
		throw std::runtime_error("error_page: path cannot be empty");
	for (size_t i = 0; i + 1 < d.args.size(); i++)
		pages[parseErrorCode(d.args[i])] = path;
}

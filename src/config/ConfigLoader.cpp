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
		server.serverNames.push_back(directive.args[i]);
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
 * Only one listening socket is supported per server today, so a warning is
 * emitted when several listen directives are declared or when none is present.
 * @param block The server block taken from the AST.
 * @param server The server block being filled.
 * @throw std::runtime_error when a directive is malformed.
 */
void	ConfigLoader::parse_directives(const ConfigBlock &block,
			ServerConfig &server)
{
	std::vector<ConfigDirective>::const_iterator	it;
	int												found = 0;

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
 * resolved at request time by the Router.
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

		std::vector<ConfigDirective>::const_iterator	dit;
		for (dit = it->directives.begin(); dit != it->directives.end(); ++dit)
		{
			if (dit->name == "autoindex")
				parseAutoindex(loc, *dit);
			else if (dit->name == "root")
				loc.root = parseRoot(*dit);
		}
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
 * @brief Applies an autoindex directive to a location.
 * @param loc The location being filled.
 * @param d The autoindex directive taken from the AST.
 * @throw std::runtime_error when the directive is malformed.
 */
void	ConfigLoader::parseAutoindex(LocationConfig &loc,
			const ConfigDirective &d)
{
	if (d.args.size() != 1)
		throw std::runtime_error("'autoindex' expects 'on' or 'off'");
	loc.autoindex = parseBool(d.args[0]);
}

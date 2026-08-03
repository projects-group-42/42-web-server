/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ConfigLoader.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: galves-a <galves-a@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/18 11:25:05 by dajesus-          #+#    #+#             */
/*   Updated: 2026/08/03 10:24:11 by galves-a         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "config/ConfigLoader.hpp"
#include "utils/Logger.hpp"
#include "utils/Utils.hpp"
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
	this->_config = copy._config;
	this->_tree = copy._tree;
}

ConfigLoader &ConfigLoader::operator=(const ConfigLoader &other)
{
	if (this != &other)
	{
		_file_path = other._file_path;
		_config = other._config;
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
 * @brief Loads the configuration file and builds the ServerConfig.
 * Reads the raw file, tokenizes it, builds the AST and extracts the listen and
 * server_name directives from it.
 * @return The populated ServerConfig.
 * @throw std::runtime_error when the file cannot be read or is invalid.
 */
ServerConfig ConfigLoader::loader(void)
{
	Lexer				lexer(configPath());
	std::vector<Token>	tokens = lexer.tokenize();
	ConfigParser		parser(tokens);

	_tree = parser.parse();
	parse_listen();
	parse_server_names();
	parse_locations();
	return (_config);
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
 * @brief Applies the arguments of a single listen directive to the config.
 * Accepts either "port" or "host:port", and rejects extra arguments as well as
 * any token carrying more than one colon.
 * @param args The argument list of the listen directive.
 * @throw std::runtime_error when the directive is malformed.
 */
void	ConfigLoader::applyListen(const std::vector<std::string> &args)
{
	if (args.empty())
		throw std::runtime_error("listen directive requires an argument");
	if (args.size() > 1)
		throw std::runtime_error("listen: expects a single host:port argument");

	const std::string	&value = args[0];
	size_t				colon = value.find(':');

	if (colon == std::string::npos)
	{
		_config.port = parsePort(value);
		return ;
	}
	if (value.find(':', colon + 1) != std::string::npos)
		throw std::runtime_error("listen: malformed host:port '" + value + "'");
	_config.host = parseHost(value.substr(0, colon));
	_config.port = parsePort(value.substr(colon + 1));
}

/**
 * @brief Walks the AST and applies every listen directive found in a server
 * block. Only one listening socket is supported today, so a warning is emitted
 * when several are declared or when none is present.
 * @throw std::runtime_error when a listen directive is malformed.
 */
void ConfigLoader::parse_listen(void)
{
	std::vector<ConfigBlock>::const_iterator	block_it;
	int											found = 0;

	for (block_it = _tree.children.begin(); block_it != _tree.children.end(); ++block_it)
	{
		if (block_it->name != "server")
			continue;

		std::vector<ConfigDirective>::const_iterator it;
		for (it = block_it->directives.begin(); it != block_it->directives.end(); ++it)
		{
			if (it->name != "listen")
				continue;
			applyListen(it->args);
			found++;
		}
	}

	std::ostringstream	oss;
	if (found == 0)
	{
		oss << "no listen directive found, using default "
			<< _config.host << ":" << _config.port;
		Logger::warning(oss.str());
		return ;
	}
	if (found > 1)
	{
		oss << found << " listen directives found, only the last one is used ("
			<< _config.host << ":" << _config.port << ")";
		Logger::warning(oss.str());
	}
}

/**
 * @brief Applies the arguments of a single server_name directive to the config.
 * Names are stored lowercased because the Host header is case-insensitive, so
 * the match done at request time can compare them directly.
 * @param args The argument list of the server_name directive.
 * @throw std::runtime_error when the directive carries no usable name.
 */
void	ConfigLoader::applyServerName(const std::vector<std::string> &args)
{
	std::vector<std::string>::const_iterator	it;

	if (args.empty())
		throw std::runtime_error("server_name directive requires an argument");
	for (it = args.begin(); it != args.end(); ++it)
	{
		if (it->empty())
			throw std::runtime_error("server_name: empty name");
		_config.serverNames.push_back(toLower(*it));
	}
}

/**
 * @brief Walks the AST and collects every server_name declared in a server
 * block. Repeated directives accumulate, mirroring NGINX. Only one server is
 * supported today, so a warning is emitted when several blocks declare names
 * and they end up merged into a single list.
 * @throw std::runtime_error when a server_name directive is malformed.
 */
void ConfigLoader::parse_server_names(void)
{
	std::vector<ConfigBlock>::const_iterator	block_it;
	int											blocks = 0;

	for (block_it = _tree.children.begin(); block_it != _tree.children.end(); ++block_it)
	{
		if (block_it->name != "server")
			continue;

		std::vector<ConfigDirective>::const_iterator	it;
		bool										declared = false;
		for (it = block_it->directives.begin(); it != block_it->directives.end(); ++it)
		{
			if (it->name != "server_name")
				continue;
			applyServerName(it->args);
			declared = true;
		}
		if (declared)
			blocks++;
	}

	if (blocks > 1)
	{
		std::ostringstream	oss;
		oss << blocks << " server blocks declare server_name, but only one "
			<< "server is supported, so every name was merged into one list";
		Logger::warning(oss.str());
	}
}

void ConfigLoader::parse_locations(void)
{
	std::vector<ConfigBlock>::const_iterator block_it;
	for (block_it = _tree.children.begin();
	     block_it != _tree.children.end(); ++block_it)
	{
		if (block_it->name != "server")
			continue;

		std::vector<ConfigBlock>::const_iterator it;
		for (it = block_it->children.begin();
		     it != block_it->children.end(); ++it)
		{
			if (it->name != "location")
				continue;

			if (it->args.empty())
				throw std::runtime_error(
				    "location block requires a path argument");

			LocationConfig loc(it->args[0]);

			std::vector<ConfigDirective>::const_iterator dit;
			for (dit = it->directives.begin();
			     dit != it->directives.end(); ++dit)
			{
				if (dit->name == "autoindex")
					parseAutoindex(loc, *dit);
			}

			_config.locations.push_back(loc);
		}
	}
}

bool ConfigLoader::parseBool(const std::string &s)
{
	if (s == "on")
		return (true);
	if (s == "off")
		return (false);
	throw std::runtime_error(
	    "expected 'on' or 'off', got: '" + s + "'");
}

void ConfigLoader::parseAutoindex(LocationConfig &loc,
                                  const ConfigDirective &d)
{
	if (d.args.size() != 1)
		throw std::runtime_error(
		    "'autoindex' expects 'on' or 'off'");
	loc.autoindex = parseBool(d.args[0]);
}

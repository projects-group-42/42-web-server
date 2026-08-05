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
#include <stdexcept>
#include <iostream>

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

std::vector<ServerConfig> ConfigLoader::loader(void)
{
	Lexer lexer(configPath());
	std::vector<Token> tokens = lexer.tokenize();
	ConfigParser parser(tokens);
	_tree = parser.parse();

	// Limpa a lista antes de começar (boa prática caso chame o loader duas vezes)
	_servers.clear();
	std::vector<ConfigBlock>::const_iterator block_it;
	for (block_it = _tree.children.begin(); block_it != _tree.children.end(); ++block_it)
	{
		if (block_it->name != "server")
			continue;
		ServerConfig current_server;
		std::vector<ConfigDirective>::const_iterator it;
		for (it = block_it->directives.begin(); it != block_it->directives.end(); ++it)
		{
			if (it->name == "listen")
				parse_listen(*it, current_server);
			else if (it->name == "root")
			{
				parse_root(*it, current_server);
			}
			else if (it->name == "server_name")
			{
				parse_names(*it, current_server);
			}
			else if (it->name == "index")
			{
				parse_index(*it, current_server);
			}
			else if (it->name == "error_page")
				parse_error_page(*it, current_server);
			else if (it->name == "client_max_body_size")
				parse_client_max_body_size(*it, current_server);
		}
		std::vector<ConfigBlock>::const_iterator child_it;
		for (child_it = block_it->children.begin(); child_it != block_it->children.end(); ++child_it)
		{
			if (child_it->name == "location")
			{
				LocationConfig location = parse_location(*child_it);
				current_server.locations.push_back(location);
			}
		}
		_servers.push_back(current_server);
	}
	return (_servers);
}

void ConfigLoader::parse_listen(
	const ConfigDirective &directive,
	ServerConfig &server)
{
	if (directive.args.empty())
		throw std::runtime_error("listen directive requires an argument");

	size_t colon_pos = directive.args[0].find(':');
	if (colon_pos != std::string::npos)// Formato: host:port
	{
		server.host = directive.args[0].substr(0, colon_pos);
		server.port = std::atoi(directive.args[0].substr(colon_pos + 1).c_str());
	}
	else
	{
		server.port = std::atoi(directive.args[0].c_str());
	}
	if (server.port < 1 || server.port > 65535)
		throw std::runtime_error("listen port out of range (1-65535)");
}

void ConfigLoader::parse_names(
	const ConfigDirective &directive,
	ServerConfig &server)
{
	if (directive.args.empty())
		throw std::runtime_error("server-name directive requires a name");
	for (size_t i = 0; i < directive.args.size(); i++)
	{
		server.serverNames.push_back(directive.args[i]);
	}
}

void ConfigLoader::parse_root(
	const ConfigDirective &directive,
	ServerConfig &server)
{
	if (directive.args.empty())
		throw std::runtime_error("root directive requires an argument");
	server.root = directive.args[0];

}

void ConfigLoader::parse_index(
	const ConfigDirective &directive,
	ServerConfig &server)
{
	if (directive.args.empty())
		throw std::runtime_error("index directive requires an argument");
	server.index = directive.args[0];

}

void ConfigLoader::parse_error_page(
	const ConfigDirective &directive,
	ServerConfig &server)
{
	if (directive.args.size() < 2)
		throw std::runtime_error("error_page requires status code and path");

	const std::string &path = directive.args.back();
	if (path.empty())
		throw std::runtime_error("invalid error_page directive");

	for (size_t j = 0; j < directive.args.size() - 1; ++j)
	{
		const std::string &code_str = directive.args[j];
		if (code_str.empty())
			throw std::runtime_error("invalid error_page status code");

		for (size_t i = 0; i < code_str.size(); ++i)
		{
			if (!std::isdigit(code_str[i]))
				throw std::runtime_error("invalid error_page status code");
		}

		int code = std::atoi(code_str.c_str());
		if (code < 400 || code > 599)
			throw std::runtime_error("invalid error_page status code");

		server.errorPages[code] = path;
	}
}

LocationConfig ConfigLoader::parse_location(const ConfigBlock &block)
{
	if (block.args.empty())
		throw std::runtime_error("location directive requires a path");

	LocationConfig location(block.args[0]);

	std::vector<ConfigDirective>::const_iterator it;
	for (it = block.directives.begin();
		 it != block.directives.end();
		 ++it)
	{
		if (it->name == "root")
			location.root = it->args[0];
		else if (it->name == "index")
			location.index = it->args[0];
	}

	return (location);
}

void ConfigLoader::parse_client_max_body_size(
	const ConfigDirective &directive,
	ServerConfig &server)
{
	if (directive.args.empty())
		throw std::runtime_error("client_max_body_size directive requires an argument");

	size_t max_body = std::strtoul(directive.args[0].c_str(), NULL, 10);
	if (directive.args[0].find('K') != std::string::npos || directive.args[0].find('k') != std::string::npos)
	{
		max_body *= 1024;
	}
	else if (directive.args[0].find('M') != std::string::npos || directive.args[0].find('m') != std::string::npos)
	{
		max_body *= 1024 * 1024;
	}
	else if (directive.args[0].find('G') != std::string::npos || directive.args[0].find('g') != std::string::npos)
	{
		max_body *= 1024 * 1024 * 1024;
	}
	server.clientMaxBodySize = max_body;
}

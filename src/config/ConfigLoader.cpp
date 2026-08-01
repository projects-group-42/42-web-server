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

ServerConfig ConfigLoader::loader(void)
{
	//lê string bruta do arq de config
	Lexer lexer(configPath());
	//quebra o arquivo simple.conf em tokens
	std::vector<Token> tokens = lexer.tokenize();
	ConfigParser parser(tokens);
	//Consome esses tokens e monta a AST
	_tree = parser.parse();

	parse_listen();
	parse_locations();
	return (_config);
}

void ConfigLoader::parse_listen(void)
{
	std::vector<ConfigBlock>::const_iterator block_it;
	for (block_it = _tree.children.begin(); block_it != _tree.children.end(); ++block_it)
	{
		if (block_it->name != "server")
			continue;

		std::vector<ConfigDirective>::const_iterator it;
		for (it = block_it->directives.begin(); it != block_it->directives.end(); ++it)
		{
			if (it->name != "listen")
				continue;

			if (it->args.empty())
				throw std::runtime_error("listen directive requires an argument");

			size_t colon_pos = it->args[0].find(':');
			if (colon_pos != std::string::npos)// Formato: host:port
			{
				_config.host = it->args[0].substr(0, colon_pos);
				_config.port = std::atoi(it->args[0].substr(colon_pos + 1).c_str());
			}
			else
			{
				_config.port = std::atoi(it->args[0].c_str());
			}

			if (_config.port < 1 || _config.port > 65535)
				throw std::runtime_error("listen port out of range (1-65535)");
		}
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

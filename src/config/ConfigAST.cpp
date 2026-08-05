/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ConfigAST.cpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/18 11:25:05 by dajesus-          #+#    #+#             */
/*   Updated: 2026/07/31 17:59:06 by jucoelho         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

# include "config/ConfigAST.hpp"

/*
 * ConfigDirective::ConfigDirective (default constructor)
 *
 * Initializes a ConfigDirective with empty name and zero line/column.
 */
ConfigDirective::ConfigDirective(void) : name(""), line(0), column(0)
{
}

/*
 * ConfigDirective::ConfigDirective (parameterized constructor)
 *
 * @param name    The directive name (e.g., "listen", "root")
 * @param line    1-based line number where this directive appears
 * @param column  1-based column number where this directive starts
 */
ConfigDirective::ConfigDirective(const std::string &name, int line, int column)
	: name(name), line(line), column(column)
{
}

/*
 * ConfigBlock::ConfigBlock (default constructor)
 *
 * Initializes a ConfigBlock with empty name and zero line/column.
 */
ConfigBlock::ConfigBlock(void) : name(""), line(0), column(0)
{
}

/*
 * ConfigBlock::ConfigBlock (parameterized constructor)
 *
 * @param name    The block name (e.g., "server", "location")
 * @param line    1-based line number where this block starts
 * @param column  1-based column number where this block starts
 */
ConfigBlock::ConfigBlock(const std::string &name, int line, int column)
	: name(name), line(line), column(column)
{
}

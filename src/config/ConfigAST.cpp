/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ConfigAST.cpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dajesus- <dajesus-@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/18 11:25:05 by dajesus-          #+#    #+#             */
/*   Updated: 2026/07/19 16:10:17 by dajesus-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

# include "config/ConfigAST.hpp"

ConfigDirective::ConfigDirective(void) : name(""), line(0), column(0)
{
}

ConfigDirective::ConfigDirective(const std::string &name, int line, int column)
	: name(name), line(line), column(column)
{
}

ConfigBlock::ConfigBlock(void) : name(""), line(0), column(0)
{
}

ConfigBlock::ConfigBlock(const std::string &name, int line, int column)
	: name(name), line(line), column(column)
{
}

/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ConfigAST.hpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dajesus- <dajesus-@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/18 11:21:16 by dajesus-          #+#    #+#             */
/*   Updated: 2026/07/19 16:09:50 by dajesus-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CONFIGAST_HPP
# define CONFIGAST_HPP

# include <string>
# include <vector>

struct ConfigDirective
{
	std::string					name;
	std::vector<std::string>	args;
	int							line;
	int							column;

	ConfigDirective(void);
	ConfigDirective(const std::string &name, int line, int column);
};

struct ConfigBlock
{
	std::string						name;
	std::vector<std::string>		args;
	std::vector<ConfigDirective>	directives;
	std::vector<ConfigBlock>		children;
	int								line;
	int								column;

	ConfigBlock(void);
	ConfigBlock(const std::string &name, int line, int column);
};

#endif

/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Utils.cpp                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dajesus- <dajesus-@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/01 17:05:24 by jucoelho          #+#    #+#             */
/*   Updated: 2026/06/12 22:30:25 by dajesus-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

# include "utils/Utils.hpp"
# include <stdexcept>
# include <fcntl.h>
# include <cctype>
# include <iostream>

//F_SETFL = Set file(FD) flags | GET -> get flags
void setNonBlocking(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1)
		throw std::runtime_error("fcntl(F_GETFL) fail");
	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
		throw std::runtime_error("fcntl(F_SETFL, O_NONBLOCK) fail");
}

std::string toLower(const std::string &str)
{
	std::string result = str;
	for (size_t i = 0; i < result.size(); i++)
		result[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(result[i])));
	return result;
}
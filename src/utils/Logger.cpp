/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Logger.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/31 18:48:33 by jucoelho          #+#    #+#             */
/*   Updated: 2026/05/31 18:58:14 by jucoelho         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <iostream>
#include "utils/Logger.hpp"
#include "utils/Colors.hpp"

void Logger::info(const std::string &message)
{
	std::cout << BLUE << "[Info] " << message << RESET << std::endl;
}

void Logger::warning(const std::string &message)
{
	std::cout << YELLOW << "[Warning] " << message << RESET << std::endl;
}

void Logger::error(const std::string &message)
{
	std::cerr << RED << "[Error] " << message << RESET << std::endl;
}

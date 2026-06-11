/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Logger.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/31 18:42:10 by jucoelho          #+#    #+#             */
/*   Updated: 2026/05/31 18:48:19 by jucoelho         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef LOGGER_HPP
# define LOGGER_HPP

# include <string>

class Logger
{
	private:
		Logger();
		Logger(const Logger &copy);
		Logger& operator=(const Logger &other);
		~Logger();
	public:
		static void info(const std::string &message);
		static void warning(const std::string &message);
		static void error(const std::string &message);
};

#endif

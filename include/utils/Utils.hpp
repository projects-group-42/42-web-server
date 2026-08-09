/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Utils.hpp                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dajesus- <dajesus-@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/01 17:03:24 by jucoelho          #+#    #+#             */
/*   Updated: 2026/06/29 21:26:32 by dajesus-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef UTILS_HPP
#define UTILS_HPP

# include <string>
# include <ctime>

void		setNonBlocking(int fd);
bool		pathIsInsideRoot(const std::string &root, const std::string &path);
std::string	toLower(const std::string &str);
std::string	getHttpDate(void);
std::string	htmlEscape(const std::string &str);
std::string	urlEncodePath(const std::string &str);
std::string	urlEncodeSegment(const std::string &str);

#endif
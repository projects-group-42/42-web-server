/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   MultipartParser.hpp                                :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dajesus- <dajesus-@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/21 23:26:11 by dajesus-          #+#    #+#             */
/*   Updated: 2026/07/24 18:21:11 by dajesus-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef MULTIPART_PARSER_HPP
# define MULTIPART_PARSER_HPP

# include <string>
# include <vector>
# include <map>

struct MultipartPart
{
	std::map<std::string, std::string>	headers;
	std::string							name;
	std::string							filename;
	std::string							content;

	bool	isFile(void) const;
};

class MultipartParser
{
	private:
		std::vector<MultipartPart>	_parts;
		int							_errorCode;

		static std::string	trim(const std::string &str);
		static std::string	extractParam(const std::string &headerValue, const std::string &param);
		bool				parseHeaders(const std::string &block, MultipartPart &part);

	public:
		MultipartParser(void);
		~MultipartParser(void);

		static bool	extractBoundary(const std::string &contentType, std::string &boundary);

		bool		parse(const std::string &body, const std::string &boundary);

		const 		std::vector<MultipartPart>	&getParts(void) const;
		int			getErrorCode(void) const;
};

#endif

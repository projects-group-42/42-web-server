/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Router.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dajesus- <dajesus-@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/24 20:49:25 by dajesus-          #+#    #+#             */
/*   Updated: 2026/06/25 21:01:27 by dajesus-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef ROUTER_HPP
# define ROUTER_HPP

# include <string>
# include "http/StaticFileHandler.hpp"

class Router
{
	private:
		StaticFileHandler	_handler;

		// Extract method and URI path from "METHOD /path HTTP/1.1\r\n..."
		bool	parseRequestLine(const std::string &buffer,
							std::string &method,
							std::string &uriPath) const;

	public:
		Router(void);
		explicit Router(const std::string &root);
		Router(const Router &copy);
		Router &operator=(const Router &other);
		~Router(void);

		bool	route(const std::string &requestBuffer,
					std::string &response);

		void				setRoot(const std::string &root);
		void				setIndex(const std::string &index);
		const std::string	&getRoot(void) const;
};

#endif

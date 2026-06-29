/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Router.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/24 20:49:25 by dajesus-          #+#    #+#             */
/*   Updated: 2026/06/29 15:19:16 by jucoelho         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef ROUTER_HPP
# define ROUTER_HPP

# include <string>
# include "http/StaticFileHandler.hpp"
# include "http/HttpRequest.hpp"

class Router
{
	private:
		StaticFileHandler	_handler;

	public:
		Router(void);
		explicit Router(const std::string &root);
		Router(const Router &copy);
		Router &operator=(const Router &other);
		~Router(void);

		bool	route(const HttpRequest &request,
					std::string &response);

		void				setRoot(const std::string &root);
		void				setIndex(const std::string &index);
		const std::string	&getRoot(void) const;
};
//RECEBE HTTP REQUEST E DEVOLVE HTTP RESPONSER STD::STRING &RESPONSE 
#endif

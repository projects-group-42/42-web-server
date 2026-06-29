/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Router.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dajesus- <dajesus-@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/24 20:49:25 by dajesus-          #+#    #+#             */
<<<<<<< HEAD
/*   Updated: 2026/06/29 15:19:16 by jucoelho         ###   ########.fr       */
=======
/*   Updated: 2026/06/29 16:50:37 by dajesus-         ###   ########.fr       */
>>>>>>> origin/refactor/project-structure
/*                                                                            */
/* ************************************************************************** */

#ifndef ROUTER_HPP
# define ROUTER_HPP

# include <string>
# include <map>
# include "http/IRequestHandler.hpp"
# include "http/StaticFileHandler.hpp"
# include "http/HttpRequest.hpp"

class Router
{
	private:
		StaticFileHandler						_staticHandler;
		std::map<std::string, IRequestHandler*>	_handlers;

		IRequestHandler	*resolveHandler(const std::string &method,
				const std::string &uri);

	public:
		Router(void);
		explicit Router(const std::string &root);
		Router(const Router &copy);
		Router &operator=(const Router &other);
		~Router(void);

		bool	route(const HttpRequest &request,
					std::string &response);

		void	addHandler(const std::string &method,
					const std::string &path, IRequestHandler *handler);

		void				setRoot(const std::string &root);
		void				setIndex(const std::string &index);
		const std::string	&getRoot(void) const;
};
//RECEBE HTTP REQUEST E DEVOLVE HTTP RESPONSER STD::STRING &RESPONSE 
#endif

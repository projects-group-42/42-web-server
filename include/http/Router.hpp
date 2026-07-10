/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Router.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dajesus- <dajesus-@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/24 20:49:25 by dajesus-          #+#    #+#             */
/*   Updated: 2026/06/29 22:23:50 by dajesus-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef ROUTER_HPP
# define ROUTER_HPP

# include <string>
# include <map>
# include "http/IRequestHandler.hpp"
# include "http/StaticFileHandler.hpp"
# include "http/HttpRequest.hpp"
# include "http/HttpResponse.hpp"
# include "http/ResponseBuilder.hpp"

class Router
{
	private:
		StaticFileHandler						_staticHandler;
		std::map<std::string, IRequestHandler*>	_handlers;
		ResponseBuilder							_responseBuilder;

		IRequestHandler	*resolveHandler(const std::string &method,
				const std::string &uri, bool &pathFound);

	public:
		Router(void);
		explicit Router(const std::string &root);
		Router(const Router &copy);
		Router &operator=(const Router &other);
		~Router(void);

		bool	route(const HttpRequest &request,
					HttpResponse &response);

		void	addHandler(const std::string &method,
					const std::string &path, IRequestHandler *handler);

		void				setRoot(const std::string &root);
		void				setIndex(const std::string &index);
		const std::string	&getRoot(void) const;
};

#endif

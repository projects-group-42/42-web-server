/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Router.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/24 20:49:25 by dajesus-          #+#    #+#             */
/*   Updated: 2026/07/19 14:40:32 by jucoelho         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef ROUTER_HPP
# define ROUTER_HPP

# include <string>
# include <map>
# include <vector>
# include "handlers/IRequestHandler.hpp"
# include "handlers/StaticFileHandler.hpp"
# include "http/HttpRequest.hpp"
# include "http/HttpResponse.hpp"
# include "http/ResponseBuilder.hpp"
# include "config/ServerConfig.hpp"

class Router
{
	private:
		StaticFileHandler						_staticHandler;
		std::map<std::string, IRequestHandler*>	_handlers;
		ResponseBuilder							_responseBuilder;
		std::vector<LocationConfig>				_locations;
		std::string								_serverRoot;
		std::string								_serverIndex;

		IRequestHandler	*resolveHandler(const std::string &method,
				const std::string &uri, bool &pathFound,
				std::string &allow);

		void			applyLocationConfig(const std::string &uri);

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

		void	setLocations(const std::vector<LocationConfig> &locations);

		void				setRoot(const std::string &root);
		void				setIndex(const std::string &index);
		const std::string	&getRoot(void) const;
};

#endif

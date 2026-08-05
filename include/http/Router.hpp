/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Router.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: galves-a <galves-a@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/24 20:49:25 by dajesus-          #+#    #+#             */
/*   Updated: 2026/08/03 22:18:44 by galves-a         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef ROUTER_HPP
# define ROUTER_HPP

# include <string>
# include <map>
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

		IRequestHandler	*resolveHandler(const std::string &method,
				const std::string &uri, bool &pathFound,
				std::string &allow);
		const LocationConfig
						*matchLocation(const std::string &uri,
				const ServerConfig &config) const;
		std::string		resolveRoot(const std::string &uri,
				const ServerConfig &config) const;
		std::string		resolveIndex(const std::string &uri,
				const ServerConfig &config) const;
		bool			resolveAutoindex(const std::string &uri,
				const ServerConfig &config) const;
		long			resolveMaxBodySize(const std::string &uri,
				const ServerConfig &config) const;
		void			applyErrorPage(const HttpRequest &request,
				HttpResponse &response, const ServerConfig &config) const;

	public:
		Router(void);
		explicit Router(const std::string &root);
		Router(const Router &copy);
		Router &operator=(const Router &other);
		~Router(void);

		bool	route(const HttpRequest &request,
					HttpResponse &response, const ServerConfig &config);

		std::string		resolveCgiInterpreter(const std::string &uri,
					const ServerConfig &config) const;

		static bool	loadErrorPage(const ServerConfig &config,
					const std::string &root, int status,
					std::string &body, std::string &contentType);

		void	addHandler(const std::string &method,
					const std::string &path, IRequestHandler *handler);

		void				setRoot(const std::string &root);
		void				setIndex(const std::string &index);
		const std::string	&getRoot(void) const;
};

#endif

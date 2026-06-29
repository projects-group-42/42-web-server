/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   webserver.hpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dajesus- <dajesus-@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/04 19:29:30 by jucoelho          #+#    #+#             */
/*   Updated: 2026/06/29 16:51:27 by dajesus-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef WEBSERVER_HPP
# define WEBSERVER_HPP

# include "http/IRequestHandler.hpp"
# include "http/HttpRequest.hpp"
# include "http/HttpResponse.hpp"
# include "http/MimeType.hpp"
# include "http/Router.hpp"
# include "http/StaticFileHandler.hpp"
# include "network/Connection.hpp"
# include "network/Socket.hpp"
# include "server/EventLoop.hpp"
# include "utils/Colors.hpp"
# include "utils/Logger.hpp"
# include "utils/Utils.hpp"

#endif
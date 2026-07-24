/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   webserver.hpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/04 19:29:30 by jucoelho          #+#    #+#             */
/*   Updated: 2026/07/19 14:40:04 by jucoelho         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef WEBSERVER_HPP
# define WEBSERVER_HPP

# include "http/HttpRequest.hpp"
# include "http/HttpResponse.hpp"
# include "http/MimeType.hpp"
# include "http/Router.hpp"
# include "handlers/IRequestHandler.hpp"
# include "handlers/StaticFileHandler.hpp"
# include "network/Connection.hpp"
# include "network/Socket.hpp"
# include "server/EventLoop.hpp"
# include "utils/Colors.hpp"
# include "utils/Logger.hpp"
# include "utils/Utils.hpp"

#endif
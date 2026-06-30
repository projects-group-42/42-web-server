/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   IRequestHandler.hpp                                :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dajesus- <dajesus-@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/29 16:22:14 by dajesus-          #+#    #+#             */
/*   Updated: 2026/06/29 21:32:42 by dajesus-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef IREQUEST_HANDLER_HPP
# define IREQUEST_HANDLER_HPP

# include <string>

class HttpRequest;
class HttpResponse;

class IRequestHandler
{
public:
	virtual ~IRequestHandler() {}
	virtual bool handle(const HttpRequest &request,
			HttpResponse &response) = 0;
};

#endif

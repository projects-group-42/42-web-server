/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   IRequestHandler.hpp                                :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dajesus- <dajesus-@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/29 16:22:14 by dajesus-          #+#    #+#             */
/*   Updated: 2026/06/29 16:51:14 by dajesus-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef IREQUEST_HANDLER_HPP
# define IREQUEST_HANDLER_HPP

# include <string>

class HttpRequest;

class IRequestHandler
{
public:
	virtual ~IRequestHandler() {}
	virtual bool handle(const HttpRequest &request,
			std::string &response) = 0;
};

#endif

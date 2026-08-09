/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   SessionStore.hpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/08/09 00:00:00 by jucoelho          #+#    #+#             */
/*   Updated: 2026/08/09 00:00:00 by jucoelho         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef SESSION_STORE_HPP
# define SESSION_STORE_HPP

# include <map>
# include <string>
# include <ctime>

/*
 * SessionStore
 *
 * Keeps one entry per session id handed out to a client. An entry only holds
 * what the server needs to recognise a returning visitor: when it was last
 * seen and how many requests it has made. Entries older than the lifetime are
 * dropped, so a client that never comes back does not stay in memory.
 */
struct Session
{
	std::string	id;
	size_t		visits;
	time_t		lastSeen;

	Session(void);
};

class SessionStore
{
	private:
		std::map<std::string, Session>	_sessions;
		size_t							_created;
		long							_lifetime;

		void	dropExpired(void);

	public:
		SessionStore(void);
		explicit SessionStore(long lifetime);
		SessionStore(const SessionStore &copy);
		SessionStore &operator=(const SessionStore &other);
		~SessionStore(void);

		static std::string	readCookie(const std::string &header,
								const std::string &name);
		const Session		&touch(std::string &id);
		bool				has(const std::string &id) const;
		size_t				visits(const std::string &id) const;
		std::string			cookieFor(const std::string &id) const;
		size_t				size(void) const;
};

#endif

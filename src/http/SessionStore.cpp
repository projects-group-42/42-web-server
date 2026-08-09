/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   SessionStore.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/08/09 00:00:00 by jucoelho          #+#    #+#             */
/*   Updated: 2026/08/09 00:00:00 by jucoelho         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "http/SessionStore.hpp"
#include <sstream>

static const long	DEFAULT_SESSION_LIFETIME_S = 1800;

Session::Session(void) : id(""), visits(0), lastSeen(0)
{
}

SessionStore::SessionStore(void)
	: _sessions(), _created(0), _lifetime(DEFAULT_SESSION_LIFETIME_S)
{
}

SessionStore::SessionStore(long lifetime)
	: _sessions(), _created(0), _lifetime(lifetime)
{
}

SessionStore::SessionStore(const SessionStore &copy)
{
	*this = copy;
}

SessionStore &SessionStore::operator=(const SessionStore &other)
{
	if (this != &other)
	{
		_sessions = other._sessions;
		_created = other._created;
		_lifetime = other._lifetime;
	}
	return (*this);
}

SessionStore::~SessionStore(void)
{
}

/*
 * Reads one cookie out of a Cookie header. The header carries the pairs a
 * client sends back separated by "; ", and only the value of `name` is
 * wanted, so the pairs are walked one by one instead of parsing them all.
 * Returns an empty string when the cookie is not there.
 */
std::string	SessionStore::readCookie(const std::string &header,
			const std::string &name)
{
	std::string	needle = name + "=";
	size_t		start = 0;

	while (start < header.size())
	{
		size_t	end = header.find(';', start);
		size_t	stop = (end == std::string::npos) ? header.size() : end;

		while (start < stop && (header[start] == ' ' || header[start] == '\t'))
			++start;
		if (stop - start > needle.size()
			&& header.compare(start, needle.size(), needle) == 0)
			return (header.substr(start + needle.size(),
					stop - start - needle.size()));
		if (end == std::string::npos)
			break ;
		start = end + 1;
	}
	return ("");
}

/*
 * Drops the sessions nobody has used for longer than the lifetime, so the map
 * follows the clients that are actually around instead of growing forever.
 */
void	SessionStore::dropExpired(void)
{
	time_t	now = std::time(NULL);

	for (std::map<std::string, Session>::iterator it = _sessions.begin();
			it != _sessions.end(); )
	{
		if (std::difftime(now, it->second.lastSeen) > _lifetime)
			_sessions.erase(it++);
		else
			++it;
	}
}

/*
 * Records a visit. An id the store knows has its counter raised; anything else
 * (no cookie, or one this server never handed out, which is what a restarted
 * server sees) is replaced by a fresh id written back into `id`, so the caller
 * knows what to put in Set-Cookie. The id is built from the clock and a
 * counter, which is enough to tell two visitors apart without pretending to be
 * unguessable.
 */
const Session	&SessionStore::touch(std::string &id)
{
	dropExpired();

	std::map<std::string, Session>::iterator	it = _sessions.find(id);

	if (id.empty() || it == _sessions.end())
	{
		std::ostringstream	fresh;

		fresh << std::time(NULL) << "-" << ++_created;
		id = fresh.str();
		_sessions[id].id = id;
		it = _sessions.find(id);
	}
	it->second.visits += 1;
	it->second.lastSeen = std::time(NULL);
	return (it->second);
}

bool	SessionStore::has(const std::string &id) const
{
	return (_sessions.find(id) != _sessions.end());
}

/*
 * Returns how many requests the session has made, or 0 for an id the store
 * does not know.
 */
size_t	SessionStore::visits(const std::string &id) const
{
	std::map<std::string, Session>::const_iterator	it = _sessions.find(id);

	if (it == _sessions.end())
		return (0);
	return (it->second.visits);
}

/*
 * Builds the Set-Cookie value announcing a session id. The cookie is scoped to
 * the whole site and marked HttpOnly, so a script running on a page cannot
 * read it.
 */
std::string	SessionStore::cookieFor(const std::string &id) const
{
	return ("SESSIONID=" + id + "; Path=/; HttpOnly");
}

size_t	SessionStore::size(void) const
{
	return (_sessions.size());
}

/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   CgiProcess.hpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: galves-a <galves-a@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/24 00:00:00 by galves-a          #+#    #+#             */
/*   Updated: 2026/07/24 00:00:00 by galves-a         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CGI_PROCESS_HPP
# define CGI_PROCESS_HPP

# include "cgi/CgiPipes.hpp"
# include <string>
# include <vector>
# include <sys/types.h>

/*
 * CgiProcess
 *
 * Owns one in-flight CGI execution so its I/O can be driven incrementally by
 * the main poll() loop instead of blocking the server. start() forks the CGI
 * child and hands back the pipe fds the event loop registers in poll(); each
 * time a pipe reports ready the loop calls onWritable()/onReadable() for a
 * single non-blocking step, streaming the request body into the child and
 * accumulating its output. Copying is disabled because it owns a pid and pipe
 * fds that must be closed and reaped exactly once.
 */
class CgiProcess
{
	private:
		CgiPipes	_pipes;
		pid_t		_pid;
		int			_clientFd;
		std::string	_body;
		size_t		_sent;
		std::string	_output;
		bool		_writing;
		bool		_reading;
		bool		_reaped;

		CgiProcess(const CgiProcess &copy);
		CgiProcess &operator=(const CgiProcess &other);

	public:
		CgiProcess(int clientFd, const std::string &body);
		~CgiProcess(void);

		bool				start(const std::string &interpreter, const std::string &scriptPath, const std::vector<std::string> &env);
		void				onReadable(void);
		void				onWritable(void);
		void				stopWriting(void);
		int					reap(void);
		void				terminate(void);
		bool				isReading(void) const;
		bool				isWriting(void) const;
		bool				finished(void) const;
		int					clientFd(void) const;
		int					outputReadFd(void) const;
		int					bodyWriteFd(void) const;
		const std::string	&output(void) const;
};

#endif

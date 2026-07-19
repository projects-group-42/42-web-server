/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   CgiPipes.hpp                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: galves-a <galves-a@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/19 12:00:00 by galves-a          #+#    #+#             */
/*   Updated: 2026/07/19 12:00:00 by galves-a         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CGI_PIPES_HPP
# define CGI_PIPES_HPP

/*
 * CgiPipes
 *
 * Owns the two unidirectional pipes that connect the web server (parent)
 * to a CGI process (child):
 *
 *   - body pipe:   request body flows parent -> child stdin
 *   - output pipe: CGI response flows child stdout -> parent
 *
 * The class only creates the pipes and enforces the closing discipline; it
 * does not fork, dup2, execve, register the fds in poll(), or build the CGI
 * environment (those belong to later steps). Every close is idempotent (the
 * fd is set to -1 and skipped afterwards), so explicit closing and the RAII
 * destructor never double-close. Copying is disabled because two owners of
 * the same fds would close them twice.
 */
class CgiPipes
{
	private:
		int	_bodyPipe[2];
		int	_outputPipe[2];

		/*
		 * Closes fd if it is still open and marks it as closed (-1). Does
		 * nothing when fd is already -1, which keeps every close idempotent.
		 */
		void		closeFd(int &fd);

		CgiPipes(const CgiPipes &copy);

		/*
		 * Copy assignment. Declared private and left unimplemented so a
		 * CgiPipes cannot be assigned: two owners would close the same fds.
		 */
		CgiPipes &operator=(const CgiPipes &other);

	public:
		/*
		 * Default constructor. Marks every pipe end as not-yet-open (-1); no
		 * pipe is created until create() is called.
		 */
		CgiPipes(void);

		/*
		 * Destructor. Closes any pipe end still open, so no descriptor is
		 * leaked on any path.
		 */
		~CgiPipes(void);

		/*
		 * Creates the body and output pipes. On success both pipes are open
		 * and true is returned. If the second pipe cannot be created the
		 * first one is closed again before returning false, so no descriptor
		 * is leaked on failure.
		 */
		bool		create(void);

		/*
		 * Returns the read end of the body pipe, which becomes the child's
		 * stdin.
		 */
		int			bodyReadFd(void) const;

		/*
		 * Returns the write end of the body pipe, through which the parent
		 * streams the request body.
		 */
		int			bodyWriteFd(void) const;

		/*
		 * Returns the read end of the output pipe, through which the parent
		 * reads the CGI response.
		 */
		int			outputReadFd(void) const;

		/*
		 * Returns the write end of the output pipe, which becomes the child's
		 * stdout.
		 */
		int			outputWriteFd(void) const;

		/*
		 * Closes the ends that belong to the child. Called by the parent
		 * right after fork: closes the body read end and the parent's own
		 * copy of the output write end. Closing that own copy is what later
		 * lets the child's exit produce EOF on the output pipe.
		 */
		void		closeChildEnds(void);

		/*
		 * Closes the ends that belong to the parent. Called by the child
		 * right after fork: closes the body write end and the output read
		 * end, which the child never uses.
		 */
		void		closeParentEnds(void);

		/*
		 * Closes the body write end. Called by the parent once the whole
		 * request body has been sent, signalling EOF to the CGI's stdin.
		 */
		void		closeBodyWrite(void);

		/*
		 * Closes the output read end. Called by the parent once the whole CGI
		 * response has been read.
		 */
		void		closeOutputRead(void);

		/*
		 * Closes every pipe end still open. Used for cleanup on error paths.
		 */
		void		closeAll(void);
};

#endif

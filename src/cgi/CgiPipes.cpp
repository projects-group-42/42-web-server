/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   CgiPipes.cpp                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: galves-a <galves-a@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/19 12:00:00 by galves-a          #+#    #+#             */
/*   Updated: 2026/07/19 12:00:00 by galves-a         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "cgi/CgiPipes.hpp"
#include <unistd.h>

CgiPipes::CgiPipes(void)
{
	_bodyPipe[0] = -1;
	_bodyPipe[1] = -1;
	_outputPipe[0] = -1;
	_outputPipe[1] = -1;
}

CgiPipes::~CgiPipes(void)
{
	closeAll();
}

void	CgiPipes::closeFd(int &fd)
{
	if (fd != -1)
	{
		close(fd);
		fd = -1;
	}
}

bool	CgiPipes::create(void)
{
	if (pipe(_bodyPipe) == -1)
		return (false);
	if (pipe(_outputPipe) == -1)
	{
		closeFd(_bodyPipe[0]);
		closeFd(_bodyPipe[1]);
		return (false);
	}
	return (true);
}

int	CgiPipes::bodyReadFd(void) const
{
	return (_bodyPipe[0]);
}

int	CgiPipes::bodyWriteFd(void) const
{
	return (_bodyPipe[1]);
}

int	CgiPipes::outputReadFd(void) const
{
	return (_outputPipe[0]);
}

int	CgiPipes::outputWriteFd(void) const
{
	return (_outputPipe[1]);
}

void	CgiPipes::closeChildEnds(void)
{
	closeFd(_bodyPipe[0]);
	closeFd(_outputPipe[1]);
}

void	CgiPipes::closeParentEnds(void)
{
	closeFd(_bodyPipe[1]);
	closeFd(_outputPipe[0]);
}

void	CgiPipes::closeBodyWrite(void)
{
	closeFd(_bodyPipe[1]);
}

void	CgiPipes::closeOutputRead(void)
{
	closeFd(_outputPipe[0]);
}

void	CgiPipes::closeAll(void)
{
	closeFd(_bodyPipe[0]);
	closeFd(_bodyPipe[1]);
	closeFd(_outputPipe[0]);
	closeFd(_outputPipe[1]);
}

#include <cassert>
#include "cgi/CgiPipes.hpp"

int main(void)
{
	CgiPipes pipes;
	assert(pipes.create() == true);
	assert(pipes.bodyReadFd() != -1);
	assert(pipes.bodyWriteFd() != -1);
	assert(pipes.outputReadFd() != -1);
	assert(pipes.outputWriteFd() != -1);

	pipes.closeChildEnds();
	assert(pipes.bodyReadFd() == -1);
	assert(pipes.outputWriteFd() == -1);

	CgiPipes parentEnds;
	assert(parentEnds.create() == true);
	parentEnds.closeParentEnds();
	assert(parentEnds.bodyWriteFd() == -1);
	assert(parentEnds.outputReadFd() == -1);

	CgiPipes cleanup;
	assert(cleanup.create() == true);
	cleanup.closeBodyWrite();
	assert(cleanup.bodyWriteFd() == -1);
	cleanup.closeOutputRead();
	assert(cleanup.outputReadFd() == -1);
	cleanup.closeAll();
	assert(cleanup.bodyReadFd() == -1);
	assert(cleanup.bodyWriteFd() == -1);
	assert(cleanup.outputReadFd() == -1);
	assert(cleanup.outputWriteFd() == -1);

	return (0);
}

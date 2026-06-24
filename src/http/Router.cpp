#include "http/Router.hpp"
#include "http/HttpResponse.hpp"

Router::Router(void)
	: _handler("www")
{
}

Router::Router(const std::string &root)
	: _handler(root)
{
}

Router::Router(const Router &copy)
	: _handler(copy._handler)
{
}

Router &Router::operator=(const Router &other)
{
	if (this != &other)
		_handler = other._handler;
	return (*this);
}

Router::~Router(void)
{
}

void	Router::setRoot(const std::string &root)
{
	_handler.setRoot(root);
}

void	Router::setIndex(const std::string &index)
{
	_handler.setIndex(index);
}

const std::string &Router::getRoot(void) const
{
	return (_handler.getRoot());
}

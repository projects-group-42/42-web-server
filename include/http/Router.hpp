#ifndef ROUTER_HPP
# define ROUTER_HPP

# include <string>
# include "http/StaticFileHandler.hpp"

class Router
{
	private:
		StaticFileHandler	_handler;

	public:
		Router(void);
		explicit Router(const std::string &root);
		Router(const Router &copy);
		Router &operator=(const Router &other);
		~Router(void);

		void				setRoot(const std::string &root);
		void				setIndex(const std::string &index);
		const std::string	&getRoot(void) const;
};

#endif

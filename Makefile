NAME		=	webserv
SRC			=	$(addprefix src/, $(SRC_FILES))
SRC_FILES	=	main.cpp \
				Logger.cpp \
				Socket.cpp \
				Utils.cpp \

OBJ_DIR		=	obj
OBJ			=	$(SRC:%.cpp=$(OBJ_DIR)/%.o)

CXX			=	c++
CXXFLAGS	=	-std=c++98 -Wall -Wextra -Werror -I include
VFLAGS		=	--leak-check=full --show-leak-kinds=all --track-origins=yes

all: $(NAME)

$(NAME): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(OBJ_DIR)/%.o : %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) -c $< -o $@

val: $(NAME)
	valgrind $(VFLAGS) ./$(NAME)

clean:
	rm -rf $(OBJ_DIR)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re
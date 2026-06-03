NAME		= webserv

SRC_FILES	= main.cpp \
			  utils/Logger.cpp
SRC			= $(addprefix src/, $(SRC_FILES))

OBJ_DIR		= obj
OBJ			= $(SRC:%.cpp=$(OBJ_DIR)/%.o)
DEP			= $(OBJ:.o=.d)

CXX			= c++
CXXFLAGS	= -std=c++98 -Wall -Wextra -Werror -I include
DEPFLAGS	= -MMD -MP
VFLAGS		= --leak-check=full --show-leak-kinds=all --track-origins=yes

all: $(NAME)

$(NAME): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(OBJ_DIR)/%.o : %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) -c $< -o $@

-include $(DEP)

val: $(NAME)
	valgrind $(VFLAGS) ./$(NAME)

clean:
	rm -rf $(OBJ_DIR)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re val
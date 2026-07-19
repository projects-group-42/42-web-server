NAME		= webserv

SRC_FILES	= main.cpp \
			  network/Socket.cpp \
			  network/Connection.cpp \
			  server/EventLoop.cpp \
			  utils/Logger.cpp \
			  utils/Utils.cpp \
			  http/HttpRequest.cpp \
			  http/HttpResponse.cpp \
			  http/MimeType.cpp \
			  http/RequestParser.cpp \
			  http/ResponseBuilder.cpp \
			  http/IRequestHandler.cpp \
			  http/StaticFileHandler.cpp \
			  http/Router.cpp \
			  cgi/CgiHandler.cpp \
			  cgi/CgiPipes.cpp \
			  config/Lexer.cpp
			  config/Lexer.cpp \
			  config/ConfigAST.cpp \
			  config/ConfigParser.cpp
SRC			= $(addprefix src/, $(SRC_FILES))

OBJ_DIR		= obj
OBJ			= $(SRC:%.cpp=$(OBJ_DIR)/%.o)
DEP			= $(OBJ:.o=.d)

TEST_SRC	= tests/mime_types_test.cpp \
			  src/http/MimeType.cpp \
			  src/utils/Utils.cpp
TEST_BIN	= test_mime

LEXER_TEST_SRC	= tests/config_lexer_test.cpp \
				  src/config/Lexer.cpp
LEXER_TEST_BIN	= test_lexer

PARSER_TEST_SRC	= tests/config_parser_test.cpp \
				  src/config/Lexer.cpp \
				  src/config/ConfigAST.cpp \
				  src/config/ConfigParser.cpp
PARSER_TEST_BIN	= test_parser

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

test: $(TEST_BIN) $(LEXER_TEST_BIN) $(PARSER_TEST_BIN)
	./$(TEST_BIN)
	./$(LEXER_TEST_BIN)
	./$(PARSER_TEST_BIN)

$(TEST_BIN): $(TEST_SRC)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(LEXER_TEST_BIN): $(LEXER_TEST_SRC)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(PARSER_TEST_BIN): $(PARSER_TEST_SRC)
	$(CXX) $(CXXFLAGS) -o $@ $^

clean:
	rm -rf $(OBJ_DIR)

fclean: clean
	rm -f $(NAME)
	rm -f $(TEST_BIN)
	rm -f $(LEXER_TEST_BIN)
	rm -f $(PARSER_TEST_BIN)

re: fclean all

.PHONY: all clean fclean re val test

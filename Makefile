NAME		= webserv
SRC			= $(shell find src -name "*.cpp")

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
			  http/MultipartParser.cpp \
			  handlers/IRequestHandler.cpp \
			  handlers/StaticFileHandler.cpp \
			  server/Router.cpp \
			  cgi/CgiHandler.cpp \
			  cgi/CgiPipes.cpp \
			  cgi/CgiProcess.cpp \
			  config/Lexer.cpp \
			  config/ConfigAST.cpp \
			  config/ConfigParser.cpp \
			  config/ConfigLoader.cpp \
			  config/ServerConfig.cpp
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

SERVER_CONFIG_TEST_SRC	= tests/server_config_test.cpp \
						  src/config/ServerConfig.cpp
SERVER_CONFIG_TEST_BIN	= test_server_config

CONFIG_LOADER_TEST_SRC	= tests/config_loader_test.cpp \
						  src/config/Lexer.cpp \
						  src/config/ConfigAST.cpp \
						  src/config/ConfigParser.cpp \
						  src/config/ConfigLoader.cpp \
						  src/config/ServerConfig.cpp \
						  src/utils/Logger.cpp
CONFIG_LOADER_TEST_BIN	= test_config_loader
CGI_TEST_SRC	= tests/cgi_handler_test.cpp \
				  src/cgi/CgiHandler.cpp \
				  src/cgi/CgiPipes.cpp \
				  src/http/HttpRequest.cpp \
				  src/http/HttpResponse.cpp \
				  src/utils/Utils.cpp
CGI_TEST_BIN	= test_cgi

MULTIPART_TEST_SRC	= tests/multipart_parser_test.cpp \
					  src/http/MultipartParser.cpp \
					  src/http/HttpRequest.cpp \
					  src/http/HttpResponse.cpp \
					  src/http/MimeType.cpp \
					  src/handlers/IRequestHandler.cpp \
					  src/handlers/StaticFileHandler.cpp \
					  src/utils/Utils.cpp
MULTIPART_TEST_BIN	= test_multipart

UPLOAD_SUITE_TEST_SRC	= tests/upload_suite_test.cpp \
						  src/http/MultipartParser.cpp \
						  src/http/HttpRequest.cpp \
						  src/http/HttpResponse.cpp \
						  src/http/MimeType.cpp \
						  src/handlers/IRequestHandler.cpp \
						  src/handlers/StaticFileHandler.cpp \
						  src/utils/Utils.cpp
UPLOAD_SUITE_TEST_BIN	= test_upload_suite

REQUEST_PARSER_TEST_SRC	= tests/request_parser_test.cpp \
						  src/http/RequestParser.cpp \
						  src/http/HttpRequest.cpp \
						  src/utils/Logger.cpp \
						  src/utils/Utils.cpp
REQUEST_PARSER_TEST_BIN	= test_request_parser

ROUTER_TEST_SRC	= tests/router_test.cpp \
				  src/server/Router.cpp \
				  src/handlers/IRequestHandler.cpp \
				  src/handlers/StaticFileHandler.cpp \
				  src/http/HttpRequest.cpp \
				  src/http/HttpResponse.cpp \
				  src/http/MimeType.cpp \
				  src/http/MultipartParser.cpp \
				  src/http/ResponseBuilder.cpp \
				  src/config/ServerConfig.cpp \
				  src/utils/Logger.cpp \
				  src/utils/Utils.cpp
ROUTER_TEST_BIN	= test_router

AUTOINDEX_TEST_SRC	= tests/autoindex_test.cpp \
					  src/handlers/IRequestHandler.cpp \
					  src/handlers/StaticFileHandler.cpp \
					  src/server/Router.cpp \
					  src/http/HttpRequest.cpp \
					  src/http/HttpResponse.cpp \
					  src/http/MimeType.cpp \
					  src/http/MultipartParser.cpp \
					  src/http/ResponseBuilder.cpp \
					  src/config/Lexer.cpp \
					  src/config/ConfigAST.cpp \
					  src/config/ConfigParser.cpp \
					  src/config/ConfigLoader.cpp \
					  src/config/ServerConfig.cpp \
					  src/utils/Logger.cpp \
					  src/utils/Utils.cpp
AUTOINDEX_TEST_BIN	= test_autoindex

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

test: $(TEST_BIN) $(LEXER_TEST_BIN) $(PARSER_TEST_BIN) $(SERVER_CONFIG_TEST_BIN) $(CONFIG_LOADER_TEST_BIN) $(CGI_TEST_BIN) $(MULTIPART_TEST_BIN) $(UPLOAD_SUITE_TEST_BIN) $(REQUEST_PARSER_TEST_BIN) $(ROUTER_TEST_BIN) $(AUTOINDEX_TEST_BIN)
	./$(TEST_BIN)
	./$(LEXER_TEST_BIN)
	./$(PARSER_TEST_BIN)
	./$(SERVER_CONFIG_TEST_BIN)
	./$(CONFIG_LOADER_TEST_BIN)
	./$(CGI_TEST_BIN)
	./$(MULTIPART_TEST_BIN)
	./$(UPLOAD_SUITE_TEST_BIN)
	./$(REQUEST_PARSER_TEST_BIN)
	./$(ROUTER_TEST_BIN)
	./$(AUTOINDEX_TEST_BIN)

$(TEST_BIN): $(TEST_SRC)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(LEXER_TEST_BIN): $(LEXER_TEST_SRC)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(PARSER_TEST_BIN): $(PARSER_TEST_SRC)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(SERVER_CONFIG_TEST_BIN): $(SERVER_CONFIG_TEST_SRC)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(CONFIG_LOADER_TEST_BIN): $(CONFIG_LOADER_TEST_SRC)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(CGI_TEST_BIN): $(CGI_TEST_SRC)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(MULTIPART_TEST_BIN): $(MULTIPART_TEST_SRC)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(UPLOAD_SUITE_TEST_BIN): $(UPLOAD_SUITE_TEST_SRC)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(REQUEST_PARSER_TEST_BIN): $(REQUEST_PARSER_TEST_SRC)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(ROUTER_TEST_BIN): $(ROUTER_TEST_SRC)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(AUTOINDEX_TEST_BIN): $(AUTOINDEX_TEST_SRC)
	$(CXX) $(CXXFLAGS) -o $@ $^

clean:
	rm -rf $(OBJ_DIR)

fclean: clean
	rm -f $(NAME)
	rm -f $(TEST_BIN)
	rm -f $(LEXER_TEST_BIN)
	rm -f $(PARSER_TEST_BIN)
	rm -f $(SERVER_CONFIG_TEST_BIN)
	rm -f $(CONFIG_LOADER_TEST_BIN)
	rm -f $(CGI_TEST_BIN)
	rm -f $(MULTIPART_TEST_BIN)
	rm -f $(UPLOAD_SUITE_TEST_BIN)
	rm -f $(REQUEST_PARSER_TEST_BIN)
	rm -f $(ROUTER_TEST_BIN)
	rm -f $(AUTOINDEX_TEST_BIN)

re: fclean all

.PHONY: all clean fclean re val test
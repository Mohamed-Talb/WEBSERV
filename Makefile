WEBSERV_SRC = main.cpp Helpers.cpp Errors.cpp\
			Server/Server.cpp Server/Listener.cpp Server/Client.cpp \
			configParser/configParser.cpp \
			HTTP/HttpHandler.cpp  HTTP/HttpRequest.cpp  HTTP/HttpResponse.cpp HTTP/FileSystem.cpp HTTP/Methods.cpp HTTP/HttpUtils.cpp \
			CGI/CGI.cpp

			
WEBSERV_OBJ = $(WEBSERV_SRC:.cpp=.o)

#COMPILING
CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98

#OUTPUT
NAME = WebServ

all: $(NAME)

$(NAME): $(WEBSERV_OBJ)
	$(CXX) $(WEBSERV_OBJ) -o $(NAME)

%.o: %.cpp
	@$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	@rm -rf $(WEBSERV_OBJ)

fclean: clean
	@rm -rf $(NAME)

re: fclean all

.PHONY: all clean fclean re
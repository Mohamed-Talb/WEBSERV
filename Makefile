WEBSERV_SRC = main.cpp Helpers.cpp\
	Server/Server.cpp \
	Server/Listener.cpp \
	Server/Client.cpp \
	Server/Session.cpp \
	configParser/configParser.cpp \
	configParser/valuesParser.cpp \
	configParser/locationHandlers.cpp \
	configParser/serverHandlers.cpp \
	configParser/configError.cpp \
	configParser/Tokenize/tokenStream.cpp \
	configParser/Tokenize/tokenizer.cpp \
	HTTP/HttpRequestParser.cpp \
	HTTP/HttpHandler.cpp \
	HTTP/HttpRequest.cpp \
	HTTP/HttpResponse.cpp \
	HTTP/HttpUtils/ContentType.cpp \
	HTTP/HttpUtils/RequestParserUtils.cpp \
	HTTP/HttpUtils/matchLocation.cpp \
	HTTP/HttpUtils/matchConfig.cpp \
	HTTP/HttpUtils/ErrorPagesBuilder.cpp \
	HTTP/HttpUtils/AutoIndexing.cpp \
	HTTP/HttpUtils/normalizePath.cpp \
	HTTP/Methods/GET.cpp \
	HTTP/Methods/POST.cpp \
	HTTP/Methods/DELETE.cpp \
	FileSystem.cpp \
	CGI/CGI.cpp \
			
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
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	@rm -rf $(WEBSERV_OBJ)

fclean: clean
	@rm -rf $(NAME)

re: fclean all

.PHONY: all clean fclean re
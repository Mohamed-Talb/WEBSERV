
WEBSERV_SRC = src/main.cpp \
	src/Helpers.cpp src/FileSystem.cpp \
	src/Server/Server.cpp \
	src/Server/Listener.cpp \
	src/Server/Client.cpp \
	src/configParser/configParser.cpp \
	src/configParser/valuesParser.cpp \
	src/configParser/locationHandlers.cpp \
	src/configParser/serverHandlers.cpp \
	src/configParser/configError.cpp \
	src/configParser/Tokenize/tokenStream.cpp \
	src/configParser/Tokenize/tokenizer.cpp \
	src/HTTP/HttpRequest/HttpRequest.cpp \
	src/HTTP/HttpRequest/RequestParser.cpp \
	src/HTTP/HttpRequest/RequestLineParser.cpp \
	src/HTTP/HttpRequest/HeadersParser.cpp \
	src/HTTP/HttpRequest/BodyParser.cpp \
	src/HTTP/HttpHandler.cpp \
	src/HTTP/HttpResponse.cpp \
	src/HTTP/HttpUtils/ContentType.cpp \
	src/HTTP/HttpUtils/RequestParserUtils.cpp \
	src/HTTP/HttpUtils/matchLocation.cpp \
	src/HTTP/HttpUtils/matchConfig.cpp \
	src/HTTP/HttpUtils/ErrorPagesBuilder.cpp \
	src/HTTP/HttpUtils/AutoIndexing.cpp \
	src/HTTP/HttpUtils/normalizePath.cpp \
	src/HTTP/Methods/GET.cpp \
	src/HTTP/Methods/POST.cpp \
	src/HTTP/Methods/DELETE.cpp \
	src/CGI/CGI.cpp

			
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
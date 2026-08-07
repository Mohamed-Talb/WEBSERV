
WEBSERV_SRC = Src/main.cpp \
	Src/Helpers.cpp Src/FileSystem.cpp \
	Src/Server/Server.cpp \
	Src/Server/Listener.cpp \
	Src/Server/Client.cpp \
	Src/configParser/configParser.cpp \
	Src/configParser/valuesParser.cpp \
	Src/configParser/locationHandlers.cpp \
	Src/configParser/serverHandlers.cpp \
	Src/configParser/configError.cpp \
	Src/configParser/Tokenize/tokenStream.cpp \
	Src/configParser/Tokenize/tokenizer.cpp \
	Src/HTTP/HttpRequest/HttpRequest.cpp \
	Src/HTTP/HttpRequest/RequestParser.cpp \
	Src/HTTP/HttpRequest/RequestLineParser.cpp \
	Src/HTTP/HttpRequest/HeadersParser.cpp \
	Src/HTTP/HttpRequest/BodyParser.cpp \
	Src/HTTP/HttpHandler.cpp \
	Src/HTTP/HttpResponse.cpp \
	Src/HTTP/HttpUtils/ContentType.cpp \
	Src/HTTP/HttpUtils/RequestParserUtils.cpp \
	Src/HTTP/HttpUtils/matchLocation.cpp \
	Src/HTTP/HttpUtils/matchConfig.cpp \
	Src/HTTP/HttpUtils/ErrorPagesBuilder.cpp \
	Src/HTTP/HttpUtils/AutoIndexing.cpp \
	Src/HTTP/HttpUtils/normalizePath.cpp \
	Src/HTTP/Methods/GET.cpp \
	Src/HTTP/Methods/POST.cpp \
	Src/HTTP/Methods/DELETE.cpp \
	Src/CGI/CGI.cpp

			
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
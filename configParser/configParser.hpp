#pragma once
#include <vector>
#include <string>
#include <map>
#include <unistd.h>
#include <stdexcept>
#include <iostream>
#include <stdlib.h>
#include <fstream>
#include <sstream>
#include "../Helpers.hpp"
#include <algorithm>


typedef std::vector<std::string> Tokens;
typedef Tokens::iterator TokenIt;

std::string parseErrorPagePath(const std::string &raw);
int         parsePort(TokenIt &it, const Tokens &tokens);
std::string parseRoot(TokenIt &it, const Tokens &tokens);
std::vector<std::string> parseIndexes(TokenIt &it, const Tokens &tokens);
std::string parseCgiExt(TokenIt &it, const Tokens &tokens);
size_t      parseBodySize(TokenIt &it, const Tokens &tokens);
std::string parseLocationPath(TokenIt &it, const Tokens &tokens);
std::string expect(TokenIt &it, const Tokens &tokens, const std::string &err);
void        expectSemicolon(TokenIt &it, const Tokens &tokens, const std::string &directive);

struct Location
{
    std::string path;
    std::string root;
    int redirectCode;
    std::string cgiExt;
    std::string cgiPath;
    std::string autoindex;
    std::string redirectTarget;
    std::string uploadEnabled;
    std::string uploadPath;
    std::vector<std::string> indexes;
    std::vector<std::string> methods;
};

struct ServerConfig
{
    int port;
    std::string host;
    std::string root;
	ssize_t client_max_body_size;
    std::vector<Location> Locations;
    std::vector<std::string> indexes;
    std::vector<std::string> serverName;
    std::map<int, std::string> errorPage;
    ServerConfig() : port(80), host("127.0.0.1"), root("./www") {}
};

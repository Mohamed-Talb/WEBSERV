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
std::string parseIndex(TokenIt &it, const Tokens &tokens);
std::string parseCgiExt(TokenIt &it, const Tokens &tokens);
size_t      parseBodySize(TokenIt &it, const Tokens &tokens);
std::string parseLocationPath(TokenIt &it, const Tokens &tokens);
std::string expect(TokenIt &it, const Tokens &tokens, const std::string &err);
void        expectSemicolon(TokenIt &it, const Tokens &tokens, const std::string &directive);

struct Location
{
    std::vector<std::string> methods;
    std::string path;
    std::string root;
    std::string autoindex;
    std::string index;
    std::string cgiPath;
    std::string cgiExt;
};

struct ServerConfig
{
    std::string host;
    int port;
    std::vector<std::string> serverName;
    std::string root;
    std::string index;
    std::vector<Location> Locations;
    std::map<int, std::string> errorPage;
	ssize_t client_max_body_size;
    ServerConfig() : host("127.0.0.1"), port(80), root("./www") {}
};

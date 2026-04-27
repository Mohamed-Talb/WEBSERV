#pragma once
#include <vector>
#include <string>
#include <map>
#include <unistd.h>
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

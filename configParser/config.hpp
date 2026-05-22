#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>
#include <vector>
#include <map>
#include <stdexcept>
#include <sys/types.h>

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
    std::map<std::string, bool> seenDirectives;
    std::vector<std::string> allowedMethods;
    Location();
    void validateLocation() const;
};

struct Listen
{
    int port;
    std::string host;
    Listen() : port(80), host("127.0.0.1"){};
};


struct ServerConfig 
{
    std::string root;
    std::vector<Listen> listens;
    ssize_t client_max_body_size;
    std::vector<Location> Locations;
    std::vector<std::string> indexes;
    std::vector<std::string> serverName;
    std::map<int, std::string> errorPage;
    std::map<std::string, bool> seenDirectives;
    ServerConfig();
    void finalizeAndValidate();
};

#endif
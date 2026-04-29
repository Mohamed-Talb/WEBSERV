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
    Location();
    void validateLocation() const;
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
    ServerConfig();
    void validate() const;
};

#endif
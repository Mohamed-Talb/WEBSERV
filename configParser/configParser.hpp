#ifndef CONFIG_ENTITIES_HPP
#define CONFIG_ENTITIES_HPP

#include <map>
#include <string>
#include <vector>
#include <limits>  
#include <sstream>
#include <cstdlib>
#include <stdexcept>
#include <algorithm>
#include <sys/types.h>
#include "../Helpers.hpp"
#include "tokenStream.hpp"
#include "valuesParser.hpp"

struct Listen
{
    int port;
    std::string host;
    Listen() : port(80), host("127.0.0.1"){};
};

struct Location 
{
    std::string path;
    std::string root;
    std::string cgiExt;
    std::string cgiPath;
    
    std::string autoindex;
    
    std::string uploadPath;
    std::string uploadEnabled;
    
    int redirectCode;
    std::string redirectTarget;

    std::vector<std::string> methods;
    std::vector<std::string> indexes;
    std::vector<std::string> allowedMethods;
    std::map<std::string, bool> seenDirectives;
    
    Location() : autoindex("off"), uploadEnabled("off") ,redirectCode(0)
    {
        allowedMethods.push_back("GET");
        allowedMethods.push_back("DELETE");
        allowedMethods.push_back("POST");
    };
};

struct ServerConfig 
{
    int port; // !!
    std::string host;// !! 
    // ..... I will leave this for now to avoid a server error
    std::string root;
    std::vector<Listen> listens;
    ssize_t client_max_body_size;
    std::vector<Location> locations;
    std::vector<std::string> indexes;
    std::vector<std::string> serverNames;
    std::map<int, std::string> errorPage;
    std::map<std::string, bool> seenDirectives;
    ServerConfig() : root("./www"), client_max_body_size(1048576)
    {
        indexes.push_back("index.html");
        serverNames.push_back(""); 
    }
};

struct CompareLocations
{
    bool operator()(const Location &a, const Location &b) const 
    {
        return a.path.size() > b.path.size();
    }
};

class ConfigParser
{

    private:
    TokenStream tokens;

    void checkDuplicate(Location &loc, const std::string &directive) const;
    void checkDuplicate(ServerConfig &conf, const std::string &directive) const;

    typedef void (ConfigParser::*ServerHandler)(ServerConfig &);
    typedef void (ConfigParser::*LocationHandler)(Location &);

    std::map<std::string, ServerHandler> serverDispatch;
    std::map<std::string, LocationHandler> locationDispatch;

    // Initialization
    void initServerDispatch();
    void initLocationDispatch();

    // Main parse flow
    void parseLocationBlock(Location &loc);
    void parseServerBlock(ServerConfig &conf);

    //
    void validateServer(ServerConfig &conf);
    void validateLocation(Location &loc);
    // Server directive handlers
    void serverRoot(ServerConfig &conf);
    void serverIndex(ServerConfig &conf);
    void serverListen(ServerConfig &conf);
    void serverLocation(ServerConfig &conf);
    void serverErrorPages(ServerConfig &conf);
    void serverNames(ServerConfig &conf);
    void serverClientMaxBodySize(ServerConfig &conf);

    // Location directive handlers
    void locationRoot(Location &loc);
    void locationIndex(Location &loc);
    void locationCgiExt(Location &loc);
    void locationUpload(Location &loc);
    void locationCgiPath(Location &loc);
    void locationMethods(Location &loc);
    void locationRedirect(Location &loc);
    void locationAutoindex(Location &loc);
    void locationUploadPath(Location &loc);

    public:
    ConfigParser();
    std::vector<ServerConfig> loadeConfig(std::string configFile);
};

#endif
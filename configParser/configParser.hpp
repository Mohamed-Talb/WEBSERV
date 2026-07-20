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
#include "./Tokenize/tokenStream.hpp"
#include "valuesParser.hpp"
#include "./configError.hpp"


void throwConfigError(const TokenStream &tokens, int errorCode);

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
    
    ssize_t client_max_body_size;

    std::vector<std::string> methods;
    std::vector<std::string> indexes;
    std::vector<std::string> allowedMethods;
    std::map<std::string, bool> seenDirectives;
    
    Location() : autoindex("off"), uploadEnabled("off") ,redirectCode(0), client_max_body_size(-1)
    {
        allowedMethods.push_back("GET");
        allowedMethods.push_back("DELETE");
        allowedMethods.push_back("POST");
    };
};

struct ServerConfig 
{
    int port;
    std::string host;
    std::string root;

    ssize_t client_max_body_size;
    std::vector<Location> locations;
    std::vector<std::string> indexes;
    std::vector<std::string> serverNames;
    std::map<int, std::string> errorPage;
    std::map<std::string, bool> seenDirectives;
    ServerConfig() : port(80), host("127.0.0.1"), root("./www"), client_max_body_size(1048576)
    {
        serverNames.push_back(""); 
        indexes.push_back("index.html");
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
    void parseServerBlock(ServerConfig &conf);

    //
    void validateLocation(Location &loc);
    void validateServer(ServerConfig &conf);

    // Server directive handlers
    void serverRoot(ServerConfig &conf);
    void serverIndex(ServerConfig &conf);
    void serverNames(ServerConfig &conf);
    void serverListen(ServerConfig &conf);
    void serverLocation(ServerConfig &conf);
    void serverErrorPages(ServerConfig &conf);
    void serverClientMaxBodySize(ServerConfig &conf);

    // Location directive handlers
    void locationRoot(Location &loc);
    void locationIndex(Location &loc);
    void locationUpload(Location &loc);
    void locationCgiExt(Location &loc);
    void locationCgiPath(Location &loc);
    void locationMethods(Location &loc);
    void locationRedirect(Location &loc);
    void locationAutoindex(Location &loc);
    void locationUploadPath(Location &loc);
    void locationClientMaxBodySize(Location &loc);

    public:
        ConfigParser();
        std::vector<ServerConfig> loadeConfig(std::string configFile);
};

#endif
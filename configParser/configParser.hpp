#ifndef CONFIG_ENTITIES_HPP
#define CONFIG_ENTITIES_HPP

#include <string>
#include <vector>
#include <map>
#include <stdexcept>
#include <cstdlib>
#include <sys/types.h>
#include "../Helpers.hpp"
#include "config.hpp"
#include <algorithm>
#include <limits>  
#include <sstream>
#include "tokenStream.hpp"
#include "valuesParser.hpp"

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

    // Server directive handlers
    void serverHost(ServerConfig &conf);
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
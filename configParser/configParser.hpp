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

std::vector<std::string> tokenize(const std::string &filepath);



struct CompareLocations
{
    bool operator()(const Location &a, const Location &b) const 
    {
        return a.path.size() > b.path.size();
    }
};

#include "tokenStream.hpp"

class ConfigParser 
{
    private:
    TokenStream tokens;

    typedef void (ConfigParser::*ServerHandler)(ServerConfig &);
    typedef void (ConfigParser::*LocationHandler)(Location &);

    std::map<std::string, ServerHandler> serverDispatch;
    std::map<std::string, LocationHandler> locationDispatch;

    void parseServerBlock(ServerConfig& conf);
    void parseLocationBlock(Location& loc);

    std::string parseRootPath();
    std::string parseLocationPathStr();
    std::vector<std::string> parseIndexesList();

    // Server Handlers
    void handleHost(ServerConfig &conf);
    void handleListen(ServerConfig &conf);
    void handleRoot(ServerConfig &conf);
    void handleServerName(ServerConfig &conf);
    void handleLocation(ServerConfig &conf);
    void handleIndex(ServerConfig &conf);
    void handleErrorPage(ServerConfig &conf);
    void handleClientMaxBodySize(ServerConfig &conf);

    // Location Handlers
    void handleLocMethods(Location &loc);
    void handleLocRoot(Location &loc);
    void handleLocAutoindex(Location &loc);
    void handleLocIndex(Location &loc);
    void handleLocCgiPath(Location &loc);
    void handleLocCgiExt(Location &loc);
    void handleLocRedirect(Location &loc);
    void handleLocUpload(Location &loc);
    void handleLocUploadPath(Location &loc);

    std::string parseRedirectTargetValue(const std::string &target);
    std::string parseCgiExtValue(const std::string &raw);
    std::string parseErrorPagePathValue(const std::string &raw);
    size_t      parseBodySizeValue(const std::string &value);
    int         parsePortValue(const std::string &value);

    public:
    ConfigParser(const std::vector<std::string> &tokens);
    std::vector<ServerConfig> parse();
};

std::vector<ServerConfig> parseConfig(const std::string &configFile);

#endif
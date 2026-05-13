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

    // Initialization
    void initServerDispatch();
    void initLocationDispatch();

    // Main parse flow
    void parseLocationBlock(Location &loc);
    void parseServerBlock(ServerConfig &conf);

    // Server directive handlers
    void handleHost(ServerConfig &conf);
    void handleRoot(ServerConfig &conf);
    void handleIndex(ServerConfig &conf);
    void handleListen(ServerConfig &conf);
    void handleLocation(ServerConfig &conf);
    void handleErrorPage(ServerConfig &conf);
    void handleServerName(ServerConfig &conf);
    void handleClientMaxBodySize(ServerConfig &conf);

    // Location directive handlers
    void handleLocRoot(Location &loc);
    void handleLocIndex(Location &loc);
    void handleLocCgiExt(Location &loc);
    void handleLocUpload(Location &loc);
    void handleLocCgiPath(Location &loc);
    void handleLocMethods(Location &loc);
    void handleLocRedirect(Location &loc);
    void handleLocAutoindex(Location &loc);
    void handleLocUploadPath(Location &loc);

    // Specific value parsers / normalizers
    std::string parseCgiPathValue();
    std::string parseLocationPath();
    std::string parseFilesystemPath();
    std::vector<std::string> parseIndexesList();
    int parsePortValue(const std::string &value);
    size_t parseBodySizeValue(const std::string &value);
    std::string parseCgiExtValue(const std::string &raw);
    std::string parseErrorPagePathValue(const std::string &raw);
    std::string parseRedirectTargetValue(const std::string &target);
    std::vector<std::string> parseWordListUntilSemicolon(const std::string &directiveName);

    public:
    std::vector<ServerConfig> parse();
    ConfigParser(const std::vector<std::string> &tokens);
};

std::vector<ServerConfig> parseConfig(const std::string &configFile);

#endif
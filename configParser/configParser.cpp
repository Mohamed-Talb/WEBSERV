#include "configParser.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cstdlib>
#include <algorithm>
#include "../Helpers.hpp"

void ConfigParser::initServerDispatch()
{
    serverDispatch["listen"] = &ConfigParser::serverListen;
    serverDispatch["root"] = &ConfigParser::serverRoot;
    serverDispatch["server_name"] = &ConfigParser::serverNames;
    serverDispatch["location"] = &ConfigParser::serverLocation;
    serverDispatch["index"] = &ConfigParser::serverIndex;
    serverDispatch["error_page"] = &ConfigParser::serverErrorPages;
    serverDispatch["client_max_body_size"] = &ConfigParser::serverClientMaxBodySize;
}

void ConfigParser::initLocationDispatch()
{
    locationDispatch["methods"] = &ConfigParser::locationMethods;
    locationDispatch["root"] = &ConfigParser::locationRoot;
    locationDispatch["autoindex"] = &ConfigParser::locationAutoindex;
    locationDispatch["index"] = &ConfigParser::locationIndex;
    locationDispatch["cgi_path"] = &ConfigParser::locationCgiPath;
    locationDispatch["cgi_ext"] = &ConfigParser::locationCgiExt;
    locationDispatch["return"] = &ConfigParser::locationRedirect;
    locationDispatch["upload"] = &ConfigParser::locationUpload;
    locationDispatch["upload_path"] = &ConfigParser::locationUploadPath;
}

ConfigParser::ConfigParser()
{
    initServerDispatch();
    initLocationDispatch();
}

 
void ConfigParser::parseLocationBlock(Location &loc)
{

    loc.path = valuesParser::parseLocationPath(this->tokens);

    if (tokens.expect("Expected '{'") != "{")
        throw std::runtime_error("Expected '{'");
    while (tokens.hasMore())
    {
        if (tokens.current() == "}")
        {
            tokens.expect("Expected '}'");
            loc.validateLocation();
            return;
        }
        std::string key = tokens.current();
        std::map<std::string, LocationHandler>::iterator handler;
        handler = locationDispatch.find(key);

        if (handler == locationDispatch.end())
            throw std::runtime_error("Unknown location directive: " + key);

        (this->*(handler->second))(loc);
    }
    throw std::runtime_error("Unclosed location block");
}

void ConfigParser::parseServerBlock(ServerConfig &conf)
{
    if (tokens.expect("Expected server") != "server")
        throw std::runtime_error("Expected server block");

    if (tokens.expect("Expected '{'") != "{")
        throw std::runtime_error("Expected '{'");

    while (tokens.hasMore())
    {
        if (tokens.current() == "}")
        {
            tokens.expect("Expected '}'");
            std ::sort(conf.Locations.begin(), conf.Locations.end(), CompareLocations());
            conf.finalizeAndValidate();
            return;
        }

        std::string key = tokens.current();

        std::map<std::string, ServerHandler>::iterator handler;
        handler = serverDispatch.find(key);

        if (handler == serverDispatch.end())
            throw std::runtime_error("Unknown server directive: " + key);

        (this->*(handler->second))(conf);
    }
    throw std::runtime_error("Unclosed server block");
}

std::vector<ServerConfig> ConfigParser::loadeConfig(std::string configFile)
{
    
    tokens = TokenStream(configFile);
    std::vector<ServerConfig> servers;
    if (!tokens.hasMore())
        throw std::runtime_error("Empty config file");
    while (tokens.hasMore())
    {
        if (tokens.current() != "server")
        {
            std::cout << tokens.current();
            throw std::runtime_error("Expected server block");
        }
        ServerConfig conf;
        parseServerBlock(conf);
        servers.push_back(conf);
    }
    return servers;
}
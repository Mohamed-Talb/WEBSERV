#include "configParser.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cstdlib>
#include <algorithm>
#include "../Helpers.hpp"

void ConfigParser::validateLocation(Location &loc)
{
    if (loc.uploadEnabled == "on" && loc.uploadPath.empty())
        throw std::runtime_error("upload on requires upload_path");

    if (loc.uploadEnabled == "off" && !loc.uploadPath.empty())
        throw std::runtime_error("upload_path set but upload is off");

    if (!loc.cgiExt.empty() && loc.cgiPath.empty())
        throw std::runtime_error("cgi_ext requires cgi_path");

    if (!loc.cgiPath.empty() && loc.cgiExt.empty())
        throw std::runtime_error("cgi_path requires cgi_ext");

    if (loc.autoindex != "on" && loc.autoindex != "off")
        throw std::runtime_error("autoindex must be 'on' or 'off'");

    if (loc.redirectCode != 0)
    {
        if (loc.redirectCode != 301 && loc.redirectCode != 302)
            throw std::runtime_error("Invalid redirect code");

        if (loc.redirectTarget.empty())
            throw std::runtime_error("Missing redirect target");

        if (loc.redirectTarget.find("..") != std::string::npos)
            throw std::runtime_error("Invalid redirect target");

        if (loc.redirectTarget[0] != '/' && loc.redirectTarget.find("http://") != 0 && loc.redirectTarget.find("https://") != 0)
        {
            throw std::runtime_error("redirect target must be path or URL");
        }
    }
}

void ConfigParser::checkDuplicate(ServerConfig &conf, const std::string &directive) const
{
    if (conf.seenDirectives[directive])
        throw std::runtime_error("duplicate " + directive + " directive in server block");
    
    conf.seenDirectives[directive] = true;
}

void ConfigParser::checkDuplicate(Location &loc, const std::string &directive) const
{
    if (loc.seenDirectives[directive])
        throw std::runtime_error("duplicate " + directive + " directive in location block");
    
    loc.seenDirectives[directive] = true;
}

void ConfigParser::validateServer(ServerConfig &conf) 
{
    if (conf.listens.empty())
    {
        conf.listens.push_back(Listen());
    }
    conf.host = conf.listens[0].host;
    conf.port = conf.listens[0].port;
    for (size_t i = 0; i < conf.Locations.size(); ++i)
    {
        Location &loc = conf.Locations[i];
        if (loc.root.empty()) 
        {
            loc.root = conf.root;
        }
        if (loc.indexes.empty()) 
        {
            loc.indexes = conf.indexes;
        }
        validateLocation(loc);
    }
}

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
            validateLocation(loc);
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
            validateServer(conf);
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
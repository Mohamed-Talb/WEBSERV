#include "configParser.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cstdlib>
#include <algorithm>
#include "../Helpers.hpp"

ConfigParser::ConfigParser(const std::vector<std::string> &tokens) : tokens(tokens)
{
    serverDispatch["host"] = &ConfigParser::handleHost;
    serverDispatch["listen"] = &ConfigParser::handleListen;
    serverDispatch["root"] = &ConfigParser::handleRoot;
    serverDispatch["server_name"] = &ConfigParser::handleServerName;
    serverDispatch["location"] = &ConfigParser::handleLocation;
    serverDispatch["index"] = &ConfigParser::handleIndex;
    serverDispatch["error_page"] = &ConfigParser::handleErrorPage;
    serverDispatch["client_max_body_size"] = &ConfigParser::handleClientMaxBodySize;

    locationDispatch["methods"] = &ConfigParser::handleLocMethods;
    locationDispatch["root"] = &ConfigParser::handleLocRoot;
    locationDispatch["autoindex"] = &ConfigParser::handleLocAutoindex;
    locationDispatch["index"] = &ConfigParser::handleLocIndex;
    locationDispatch["cgi_path"] = &ConfigParser::handleLocCgiPath;
    locationDispatch["cgi_ext"] = &ConfigParser::handleLocCgiExt;
    locationDispatch["redirect"] = &ConfigParser::handleLocRedirect;
    locationDispatch["upload"] = &ConfigParser::handleLocUpload;
    locationDispatch["upload_path"] = &ConfigParser::handleLocUploadPath;
}

 
void ConfigParser::parseLocationBlock(Location &loc)
{
    loc.path = parseLocationPathStr();

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
            conf.validate();
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

std::vector<ServerConfig> ConfigParser::parse()
{
    std::vector<ServerConfig> servers;
    if (!tokens.hasMore())
        throw std::runtime_error("Empty config file");
    while (tokens.hasMore())
    {
        if (tokens.current() != "server")
            throw std::runtime_error("Expected server block");

        ServerConfig conf;
        parseServerBlock(conf);

        servers.push_back(conf);
    }
    return servers;
}
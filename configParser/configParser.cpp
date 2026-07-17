#include "configParser.hpp"

#include <algorithm>

void ConfigParser::validateLocation(Location &loc)
{
    if (loc.uploadEnabled == "on" && loc.uploadPath.empty())
    {
        throw std::runtime_error(
            "Config error in location '" + loc.path
            + "': upload is enabled but upload_path is missing"
        );
    }

    if (loc.uploadEnabled == "off" && !loc.uploadPath.empty())
    {
        throw std::runtime_error(
            "Config error in location '" + loc.path
            + "': upload_path is set but upload is disabled"
        );
    }

    if (!loc.cgiExt.empty() && loc.cgiPath.empty())
    {
        throw std::runtime_error(
            "Config error in location '" + loc.path
            + "': cgi_ext requires cgi_path"
        );
    }

    if (!loc.cgiPath.empty() && loc.cgiExt.empty())
    {
        throw std::runtime_error(
            "Config error in location '" + loc.path
            + "': cgi_path requires cgi_ext"
        );
    }
}

void ConfigParser::checkDuplicate(ServerConfig &conf, const std::string &directive) const
{
    if (conf.seenDirectives.find(directive) != conf.seenDirectives.end())
        throwConfigError(tokens, ERR_DUPLICATE_DIRECTIVE);

    conf.seenDirectives[directive] = true;
}

void ConfigParser::checkDuplicate(Location &loc, const std::string &directive) const
{
    if (loc.seenDirectives.find(directive) != loc.seenDirectives.end())
        throwConfigError(tokens, ERR_DUPLICATE_DIRECTIVE);

    loc.seenDirectives[directive] = true;
}

void ConfigParser::validateServer(ServerConfig &conf)
{
    for (size_t i = 0; i < conf.locations.size(); ++i)
    {
        Location &loc = conf.locations[i];

        if (loc.root.empty())
            loc.root = conf.root;

        if (loc.indexes.empty())
            loc.indexes = conf.indexes;
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

void ConfigParser::parseServerBlock(ServerConfig &conf)
{
    tokens.expect("server");
    tokens.expect("{");

    while (!tokens.atEnd())
    {
        if (tokens.peekCurrent().text == "}")
        {
            std::sort(conf.locations.begin(), conf.locations.end(), CompareLocations());
            validateServer(conf);
            tokens.expect("}");
            return;
        }
        const std::string &directive = tokens.peekCurrent().text;

        std::map<std::string, ServerHandler>::iterator handler;
        handler = serverDispatch.find(directive);

        if (handler == serverDispatch.end())
            throwConfigError(tokens, ERR_UNKNOWN_DIRECTIVE);

        (this->*(handler->second))(conf);
    }
    throwConfigError(tokens, ERR_UNCLOSED_SERVER);
}

std::vector<ServerConfig> ConfigParser::loadeConfig(std::string configFile)
{
    tokens = TokenStream(configFile);
    std::vector<ServerConfig> servers;

    if (tokens.atEnd())
        throwConfigError(tokens, ERR_EMPTY_CONFIG);

    while (!tokens.atEnd())
    {
        ServerConfig conf;

        parseServerBlock(conf);
        servers.push_back(conf);
    }

    return servers;
}
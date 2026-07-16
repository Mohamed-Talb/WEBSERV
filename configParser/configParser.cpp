#include "configParser.hpp"
#include "../Errors.hpp"

#include <algorithm>

void ConfigParser::validateLocation(Location &loc)
{
    if ((loc.uploadEnabled == "on" && loc.uploadPath.empty())
        || (loc.uploadEnabled == "off" && !loc.uploadPath.empty()))
    {
        configError(ERR_INVALID_UPLOAD_CONFIG);
    }

    if (loc.cgiExt.empty() != loc.cgiPath.empty())
        configError(ERR_INVALID_CGI_CONFIG);
}

void ConfigParser::checkDuplicate(ServerConfig &conf, const std::string &directive) const
{
    if (conf.seenDirectives[directive])
        configError(ERR_DUPLICATE_DIRECTIVE);

    conf.seenDirectives[directive] = true;
}

void ConfigParser::checkDuplicate(Location &loc, const std::string &directive) const
{
    if (loc.seenDirectives[directive])
        configError(ERR_DUPLICATE_DIRECTIVE);

    loc.seenDirectives[directive] = true;
}

void ConfigParser::validateServer(ServerConfig &conf)
{
    if (conf.listens.empty())
        conf.listens.push_back(Listen());

    conf.host = conf.listens[0].host;
    conf.port = conf.listens[0].port;

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

void ConfigParser::parseLocationBlock(Location &loc)
{
    loc.path = valuesParser::parseLocationPath(tokens);
    tokens.expect("{");

    while (!tokens.atEnd())
    {
        if (tokens.peek().text == "}")
        {
            validateLocation(loc);
            tokens.expect("}");
            return;
        }
        const std::string &directive = tokens.peek().text;

        std::map<std::string, LocationHandler>::iterator handler;
        handler = locationDispatch.find(directive);

        if (handler == locationDispatch.end())
            configError(ERR_UNKNOWN_LOCATION_DIRECTIVE);

        (this->*(handler->second))(loc);
    }

    configError(ERR_UNCLOSED_LOCATION);
}

void ConfigParser::parseServerBlock(ServerConfig &conf)
{
    if (tokens.atEnd() || tokens.peek().text != "server")
        configError(ERR_EXPECTED_SERVER);

    tokens.expect("server");
    tokens.expect("{");

    while (!tokens.atEnd())
    {
        if (tokens.peek().text == "}")
        {
            std::sort(conf.locations.begin(), conf.locations.end(), CompareLocations());
            validateServer(conf);

            tokens.expect("}");
            return;
        }

        const std::string &directive = tokens.peek().text;

        std::map<std::string, ServerHandler>::iterator handler;
        handler = serverDispatch.find(directive);

        if (handler == serverDispatch.end())
            configError(ERR_UNKNOWN_SERVER_DIRECTIVE);

        (this->*(handler->second))(conf);
    }

    configError(ERR_UNCLOSED_SERVER);
}

std::vector<ServerConfig> ConfigParser::loadeConfig(std::string configFile)
{
    tokens = TokenStream(configFile);

    std::vector<ServerConfig> servers;

    if (tokens.atEnd())
        configError(ERR_EMPTY_CONFIG);

    while (!tokens.atEnd())
    {
        ServerConfig conf;

        parseServerBlock(conf);
        servers.push_back(conf);
    }

    return servers;
}
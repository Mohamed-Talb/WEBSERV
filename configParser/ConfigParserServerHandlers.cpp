#include "configParser.hpp"

void ConfigParser::handleHost(ServerConfig &conf)
{
    if (conf.seenDirectives["host"])
        throw std::runtime_error("duplicate host directive");
    conf.seenDirectives["host"] = true;
    tokens.expect("Expected host");
    conf.host = tokens.expect("Missing host");
    if (!isValidHost(conf.host))
        throw std::runtime_error("Invalid host: " + conf.host);
    tokens.expectSemicolon("host");
}

void ConfigParser::handleListen(ServerConfig &conf)
{
    if (conf.seenDirectives["listen"])
        throw std::runtime_error("duplicate listen directive");

    conf.seenDirectives["listen"] = true;
    tokens.expect("Expected listen");

    std::string value = tokens.expect("Missing port");
    conf.port = parsePortValue(value);

    tokens.expectSemicolon("listen");
}

void ConfigParser::handleRoot(ServerConfig &conf)
{
    if (conf.seenDirectives["root"])
        throw std::runtime_error("duplicate root directive");

    conf.seenDirectives["root"] = true;
    tokens.expect("Expected root");

    conf.root = parseFilesystemPath();

    tokens.expectSemicolon("root");
}

void ConfigParser::handleServerName(ServerConfig &conf)
{
    if (conf.seenDirectives["server_name"])
        throw std::runtime_error("duplicate server_name directive");

    conf.seenDirectives["server_name"] = true;

    tokens.expect("Expected server_name");

    conf.serverName = parseWordListUntilSemicolon("server_name");
}

void ConfigParser::handleLocation(ServerConfig &conf)
{
    tokens.expect("Expected location");

    Location loc;
    parseLocationBlock(loc);
    for (size_t i = 0; i < conf.Locations.size(); ++i)
    {
        if (conf.Locations[i].path == loc.path)
            throw std::runtime_error("Duplicate location: " + loc.path);
    }
    conf.Locations.push_back(loc);
}

void ConfigParser::handleIndex(ServerConfig &conf)
{
    if (conf.seenDirectives["index"])
        throw std::runtime_error("duplicate index directive");

    conf.seenDirectives["index"] = true;    
    tokens.expect("Expected index");

    conf.indexes = parseIndexesList();
}

void ConfigParser::handleErrorPage(ServerConfig &conf)
{
    std::vector<std::string> values;

    tokens.expect("Expected error_page");

    while (tokens.hasMore() && tokens.current() != ";")
        values.push_back(tokens.expect("Missing error_page value"));

    if (!tokens.hasMore())
        throw std::runtime_error("Missing ';' after error_page");

    if (values.size() < 2)
        throw std::runtime_error("Invalid error_page syntax: missing codes or path");

    std::string path = parseErrorPagePathValue(values.back());

    for (size_t i = 0; i < values.size() - 1; ++i)
    {
        if (!isOnlyDigits(values[i]))
            throw std::runtime_error("Invalid error code in config: " + values[i]);

        unsigned long code = static_cast<unsigned long>(std::atol(values[i].c_str()));

        if (code < 300 || code > 599)
            throw std::runtime_error("Invalid error code in config: " + values[i]);

        int errorCode = static_cast<int>(code);

        if (conf.errorPage.count(errorCode))
            throw std::runtime_error("duplicate error_page code: " + values[i]);

        conf.errorPage[errorCode] = path;
    }
    tokens.expectSemicolon("error_page");
}

void ConfigParser::handleClientMaxBodySize(ServerConfig &conf)
{
    if (conf.seenDirectives["client_max_body_size"])
        throw std::runtime_error("duplicate client_max_body_size directive");

    conf.seenDirectives["client_max_body_size"] = true;
    tokens.expect("Expected client_max_body_size");

    std::string value = tokens.expect("Missing body size value");
    conf.client_max_body_size = parseBodySizeValue(value);
    if (conf.client_max_body_size == 0)
        throw std::runtime_error("Invalide client_max_body_size");
    tokens.expectSemicolon("client_max_body_size");
}

#include "configParser.hpp"
#include "../Errors.hpp"

#include <algorithm>
#include <cstdlib>
#include <sstream>

void ConfigParser::serverListen(ServerConfig &conf)
{
    checkDuplicate(conf, "listen");
    tokens.expect("listen");

    std::string value = tokens.expectValue("listen value").text;
    Listen currentListen;

    if (value.find(':') == std::string::npos)
    {
        if (value.find('.') != std::string::npos)
            value += ":80";
        else
            value = "0.0.0.0:" + value;
    }

    size_t colonPosition = value.find(':');

    if (colonPosition == 0 || colonPosition == value.size() - 1)
        configError(ERR_INVALID_LISTEN);

    std::string host = value.substr(0, colonPosition);
    std::string port = value.substr(colonPosition + 1);

    if (host == "localhost")
        host = "127.0.0.1";

    if (!isValidHost(host))
        configError(ERR_INVALID_HOST);

    if (!isOnlyDigits(port) || port.size() > 5)
        configError(ERR_INVALID_PORT);

    int portNumber = std::atoi(port.c_str());

    if (portNumber <= 0 || portNumber > 65535)
        configError(ERR_INVALID_PORT);

    currentListen.host = host;
    currentListen.port = portNumber;

    conf.listens.push_back(currentListen);

    tokens.expectSemicolon();
}

void ConfigParser::serverRoot(ServerConfig &conf)
{
    checkDuplicate(conf, "root");
    tokens.expect("root");

    conf.root = valuesParser::parseFilesystemPath(tokens);

    tokens.expectSemicolon();
}

void ConfigParser::serverNames(ServerConfig &conf)
{
    checkDuplicate(conf, "server_name");
    tokens.expect("server_name");

    conf.serverNames.clear();

    while (!tokens.atEnd() && tokens.peek().text != ";")
    {
        std::string serverName = tokens.expectValue("server name").text;

        if (!isValidServerName(serverName))
            configError(ERR_INVALID_SERVER_NAME);

        conf.serverNames.push_back(serverName);
    }

    if (conf.serverNames.empty())
        configError(ERR_MISSING_VALUE);

    if (tokens.atEnd())
        configError(ERR_MISSING_SEMICOLON);

    tokens.expectSemicolon();
}

void ConfigParser::serverIndex(ServerConfig &conf)
{
    checkDuplicate(conf, "index");
    tokens.expect("index");

    conf.indexes = valuesParser::parseIndexesList(tokens);

    tokens.expectSemicolon();
}

void ConfigParser::serverClientMaxBodySize(ServerConfig &conf)
{
    checkDuplicate(conf, "client_max_body_size");
    tokens.expect("client_max_body_size");

    conf.client_max_body_size = valuesParser::parseBodySizeValue(tokens);

    if (conf.client_max_body_size == 0)
        configError(ERR_INVALID_BODY_SIZE);

    tokens.expectSemicolon();
}

void ConfigParser::serverErrorPages(ServerConfig &conf)
{
    tokens.expect("error_page");

    std::vector<int> codes;

    while (!tokens.atEnd() && tokens.peek().text != ";"
        && isOnlyDigits(tokens.peek().text))
    {
        std::string codeValue = tokens.expectValue("error status code").text;

        if (codeValue.size() > 3)
            configError(ERR_INVALID_ERROR_CODE);

        int errorCode = 0;
        std::istringstream stream(codeValue);

        stream >> errorCode;

        if (stream.fail() || !isValidErrorCode(errorCode))
            configError(ERR_INVALID_ERROR_CODE);

        if (std::find(codes.begin(), codes.end(), errorCode) != codes.end())
            configError(ERR_DUPLICATE_ERROR_CODE);

        if (conf.errorPage.count(errorCode) != 0)
            configError(ERR_DUPLICATE_ERROR_CODE);

        codes.push_back(errorCode);
    }

    if (codes.empty())
        configError(ERR_MISSING_VALUE);

    if (tokens.atEnd() || tokens.peek().text == ";")
        configError(ERR_MISSING_VALUE);

    std::string path = valuesParser::parseErrorPagePathValue(tokens);

    for (size_t i = 0; i < codes.size(); ++i)
        conf.errorPage[codes[i]] = path;

    tokens.expectSemicolon();
}

void ConfigParser::serverLocation(ServerConfig &conf)
{
    tokens.expect("location");

    Location location;
    location.path = valuesParser::parseLocationPath(tokens);

    for (size_t i = 0; i < conf.locations.size(); ++i)
    {
        if (conf.locations[i].path == location.path)
            configError(ERR_DUPLICATE_LOCATION);
    }

    parseLocationBlock(location);
    conf.locations.push_back(location);
}
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
        if (value.find('.') != std::string::npos
            || value == "localhost")
        {
            value += ":80";
        }
        else
        {
            value = "0.0.0.0:" + value;
        }
    }

    size_t colonPosition = value.find(':');

    if (colonPosition == 0
        || colonPosition == value.size() - 1
        || value.find(':', colonPosition + 1) != std::string::npos)
    {
        throwConfigError(tokens, ERR_INVALID_LISTEN);
    }

    std::string host = value.substr(0, colonPosition);
    std::string portValue = value.substr(colonPosition + 1);

    if (host == "localhost")
        host = "127.0.0.1";

    if (!isValidHost(host))
        throwConfigError(tokens, ERR_INVALID_HOST);

    if (!isOnlyDigits(portValue) || portValue.size() > 5)
        throwConfigError(tokens, ERR_INVALID_PORT);

    int port = std::atoi(portValue.c_str());

    if (port <= 0 || port > 65535)
        throwConfigError(tokens, ERR_INVALID_PORT);

    currentListen.host = host;
    currentListen.port = port;

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
            throwConfigError(tokens, ERR_INVALID_SERVER_NAME);

        conf.serverNames.push_back(serverName);
    }

    if (conf.serverNames.empty())
        throwConfigError(tokens, ERR_MISSING_VALUE);

    if (tokens.atEnd())
        throwConfigError(tokens, ERR_MISSING_SEMICOLON);

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
        throwConfigError(tokens, ERR_INVALID_BODY_SIZE);

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
            throwConfigError(tokens, ERR_INVALID_ERROR_CODE);

        int errorCode = 0;
        std::istringstream stream(codeValue);

        stream >> errorCode;

        if (stream.fail() || !isValidErrorCode(errorCode))
            throwConfigError(tokens, ERR_INVALID_ERROR_CODE);

        if (std::find(codes.begin(), codes.end(), errorCode) != codes.end())
            throwConfigError(tokens, ERR_DUPLICATE_ERROR_CODE);

        if (conf.errorPage.count(errorCode) != 0)
            throwConfigError(tokens, ERR_DUPLICATE_ERROR_CODE);

        codes.push_back(errorCode);
    }

    if (codes.empty())
        throwConfigError(tokens, ERR_MISSING_VALUE);

    if (tokens.atEnd() || tokens.peek().text == ";")
        throwConfigError(tokens, ERR_MISSING_VALUE);

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
            throwConfigError(tokens, ERR_DUPLICATE_LOCATION);
    }

    tokens.expect("{");

    while (!tokens.atEnd())
    {
        if (tokens.peek().text == "}")
        {
            validateLocation(location);
            tokens.expect("}");

            conf.locations.push_back(location);
            return;
        }

        const std::string &directive = tokens.peek().text;

        std::map<std::string, LocationHandler>::iterator handler;
        handler = locationDispatch.find(directive);

        if (handler == locationDispatch.end())
            throwConfigError(tokens, ERR_UNKNOWN_LOCATION_DIRECTIVE);

        (this->*(handler->second))(location);
    }
    throwConfigError(tokens, ERR_UNCLOSED_LOCATION);
}

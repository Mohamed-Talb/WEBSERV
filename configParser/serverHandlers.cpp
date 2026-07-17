#include "configParser.hpp"
#include "../Errors.hpp"

#include <algorithm>
#include <cstdlib>
#include <sstream>

void ConfigParser::serverListen(ServerConfig &conf)
{
    checkDuplicate(conf, "listen");
    tokens.expect("listen");

    const Token &valueToken = tokens.peekValue("listen value");
    std::string value = valueToken.text;

    if (value.find(':') == std::string::npos)
    {
        if (value.find('.') != std::string::npos || value == "localhost")
        {
            value += ":80";
        }
        else
        {
            value = "0.0.0.0:" + value;
        }
    }

    size_t colonPosition = value.find(':');

    if (colonPosition == 0 || colonPosition == value.size() - 1 || value.find(':', colonPosition + 1) != std::string::npos)
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

    conf.host = host;
    conf.port = port;

    tokens.consume();
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
        const Token &nameToken = tokens.peekValue("server name");

        if (!isValidServerName(nameToken.text))
            throwConfigError(tokens, ERR_INVALID_SERVER_NAME);

        conf.serverNames.push_back(nameToken.text);
        tokens.consume();
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

    conf.client_max_body_size =
        valuesParser::parseBodySizeValue(tokens);

    tokens.expectSemicolon();
}

void ConfigParser::serverErrorPages(ServerConfig &conf)
{
    tokens.expect("error_page");
    std::vector<int> codes;

    while (!tokens.atEnd() && tokens.peek().text != ";" && isOnlyDigits(tokens.peek().text))
    {
        const Token &codeToken =
            tokens.peekValue("error status code");

        int errorCode = 0;
        std::istringstream stream(codeToken.text);

        stream >> errorCode;

        if (stream.fail() || !stream.eof() || !isValidErrorCode(errorCode))
        {
            throwConfigError(tokens, ERR_INVALID_STATUS_CODE);
        }

        if (std::find(codes.begin(), codes.end(), errorCode) != codes.end())
        {
            throwConfigError(tokens, ERR_DUPLICATE_VALUE);
        }

        if (conf.errorPage.count(errorCode) != 0)
            throwConfigError(tokens, ERR_DUPLICATE_VALUE);

        codes.push_back(errorCode);
        tokens.consume();
    }

    if (codes.empty())
        throwConfigError(tokens, ERR_MISSING_VALUE);

    if (tokens.atEnd() || tokens.peek().text == ";")
        throwConfigError(tokens, ERR_MISSING_VALUE);

    std::string path =
        valuesParser::parseErrorPagePathValue(tokens);

    for (size_t i = 0; i < codes.size(); ++i)
        conf.errorPage[codes[i]] = path;

    tokens.expectSemicolon();
}


void ConfigParser::serverLocation(ServerConfig &conf)
{
    tokens.expect("location");

    std::string path =
        valuesParser::parseLocationPath(tokens);

    for (size_t i = 0; i < conf.locations.size(); ++i)
    {
        if (conf.locations[i].path == path)
            throwConfigError(tokens, ERR_DUPLICATE_LOCATION);
    }

    Location location;
    location.path = path;

    tokens.consume();
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

        const std::string &directive =
            tokens.peek().text;

        std::map<std::string, LocationHandler>::iterator handler;
        handler = locationDispatch.find(directive);

        if (handler == locationDispatch.end())
        {
            throwConfigError(tokens,ERR_UNKNOWN_DIRECTIVE);
        }
        (this->*(handler->second))(location);
    }

    throwConfigError(tokens, ERR_UNCLOSED_LOCATION);
}
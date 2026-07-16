#include "configParser.hpp"
#include "../Errors.hpp"

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
        throwError(ERR_INVALID_SYNTAX, value);

    std::string host = value.substr(0, colonPosition);
    std::string port = value.substr(colonPosition + 1);

    if (host == "localhost")
        host = "127.0.0.1";

    if (!isValidHost(host))
        throwError(ERR_INVALID_VALUE, host);

    currentListen.host = host;
    currentListen.port = valuesParser::parsePortValue(port);

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

    conf.serverNames = valuesParser::parseWordListUntilSemicolon(tokens, "server_name");

    for (size_t i = 0; i < conf.serverNames.size(); ++i)
    {
        if (!isValidServerName(conf.serverNames[i]))
        {
            throwError(
                ERR_INVALID_VALUE,
                conf.serverNames[i] + " (invalid server_name format)"
            );
        }
    }

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
    {
        throwError(
            ERR_INVALID_VALUE,
            "0 (client_max_body_size must be greater than 0)"
        );
    }

    tokens.expectSemicolon();
}

void ConfigParser::serverErrorPages(ServerConfig &conf)
{
    tokens.expect("error_page");

    std::vector<int> codes;

    while (!tokens.atEnd() && tokens.peek().text != ";")
    {
        std::string current = tokens.peek().text;

        if (isOnlyDigits(current))
        {
            int errorCode = 0;
            std::istringstream stream(current);

            stream >> errorCode;

            if (stream.fail())
                throwError(ERR_INVALID_VALUE, current);

            if (!isValidErrorCode(errorCode))
                throwError(ERR_INVALID_VALUE, current);

            codes.push_back(errorCode);
            tokens.consume();
        }
        else
        {
            if (codes.empty())
            {
                throwError(
                    ERR_MISSING_VALUE,
                    "error_page (requires at least one status code)"
                );
            }

            std::string path = valuesParser::parseErrorPagePathValue(tokens);

            for (size_t i = 0; i < codes.size(); ++i)
            {
                if (conf.errorPage.count(codes[i]) != 0)
                {
                    std::ostringstream codeText;
                    codeText << codes[i];

                    throwError(ERR_DUPLICATE_VALUE, codeText.str());
                }

                conf.errorPage[codes[i]] = path;
            }

            break;
        }
    }

    if (codes.empty())
    {
        throwError(
            ERR_MISSING_VALUE,
            "error_page (requires at least one status code)"
        );
    }

    if (tokens.atEnd())
        throwError(ERR_MISSING_SEMICOLON, "error_page");

    if (tokens.peek().text != ";")
        throwError(ERR_INVALID_SYNTAX, tokens.peek().text);

    tokens.expectSemicolon();
}

void ConfigParser::serverLocation(ServerConfig &conf)
{
    tokens.expect("location");

    Location location;
    parseLocationBlock(location);

    for (size_t i = 0; i < conf.locations.size(); ++i)
    {
        if (conf.locations[i].path == location.path)
            throwError(ERR_DUPLICATE_VALUE, location.path);
    }

    conf.locations.push_back(location);
}
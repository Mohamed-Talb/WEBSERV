#include "configParser.hpp"
#include "../Errors.hpp"

void ConfigParser::serverListen(ServerConfig &conf)
{
    this->checkDuplicate(conf, "listen");
    this->tokens.expect("Expected listen");
    
    std::string value = tokens.expect("Missing listen Value");
    Listen currListen;
    if (value.find(':') == std::string::npos)
    {
        if (value.find('.') != std::string::npos)
            value = value + ":80";
        else
            value = "0.0.0.0:" + value;
    }
    size_t colonPos = value.find(':');
    if (colonPos == 0)
        throwError(ERR_INVALID_SYNTAX, value);
    if (colonPos == value.length() - 1)
        throwError(ERR_INVALID_SYNTAX, value);
    
    std::string ipPart = value.substr(0, colonPos);
    if (ipPart == "localhost")
        ipPart = "127.0.0.1";
    if (!isValidHost(ipPart))
        throwError(ERR_INVALID_VALUE, ipPart);
        
    currListen.host = ipPart;

    std::string portPart = value.substr(colonPos + 1);
    currListen.port = valuesParser::parsePortValue(portPart); 
    conf.listens.push_back(currListen);

    this->tokens.expectSemicolon("listen");
}

void ConfigParser::serverRoot(ServerConfig &conf)
{
    this->checkDuplicate(conf, "root");
    this->tokens.expect("Expected root");
    conf.root = valuesParser::parseFilesystemPath(this->tokens);
    this->tokens.expectSemicolon("root");
}

void ConfigParser::serverNames(ServerConfig &conf)
{
    this->checkDuplicate(conf, "server_name");
    this->tokens.expect("Expected server_name");
    conf.serverNames = valuesParser::parseWordListUntilSemicolon(this->tokens, "server_name");
    for (size_t i = 0; i < conf.serverNames.size(); ++i)
    {
        if (!isValidServerName(conf.serverNames[i]))
        {
            throwError(ERR_INVALID_VALUE, conf.serverNames[i] + " (invalid server_name format)");
        }
    }
    tokens.expectSemicolon("server_name");
}

void ConfigParser::serverIndex(ServerConfig &conf)
{
    this->checkDuplicate(conf, "index");    
    this->tokens.expect("Expected index");
    conf.indexes = valuesParser::parseIndexesList(this->tokens);
    tokens.expectSemicolon("index");
}

void ConfigParser::serverClientMaxBodySize(ServerConfig &conf)
{
    this->checkDuplicate(conf, "client_max_body_size");
    this->tokens.expect("Expected client_max_body_size");

    conf.client_max_body_size = valuesParser::parseBodySizeValue(this->tokens);
    if (conf.client_max_body_size == 0)
        throwError(ERR_INVALID_VALUE, "0 (client_max_body_size must be greater than 0)");
        
    this->tokens.expectSemicolon("client_max_body_size");
}

void ConfigParser::serverErrorPages(ServerConfig &conf)
{
    this->tokens.expect("Expected error_page");

    std::vector<int> codes;

    while (this->tokens.hasMore())
    {
        std::string curr = this->tokens.current();
        
        if (isOnlyDigits(curr))
        {
            int errorCode = 0;
            std::istringstream stream(curr);
            stream >> errorCode;
            if (stream.fail())
                throwError(ERR_INVALID_VALUE, curr);

            if (!isValidErrorCode(errorCode))
                throwError(ERR_INVALID_VALUE, curr);

            codes.push_back(errorCode);
            this->tokens.expect("Expected error code"); 
        }
        else
        {
            if (codes.empty())
                throwError(ERR_MISSING_VALUE, "error_page (requires at least one status code)");

            std::string path = valuesParser::parseErrorPagePathValue(this->tokens);
            for (size_t i = 0; i < codes.size(); ++i)
            {
                if (conf.errorPage.count(codes[i]))
                {
                    std::ostringstream oss;
                    oss << codes[i];
                    throwError(ERR_DUPLICATE_VALUE, oss.str());
                }
                conf.errorPage[codes[i]] = path;
            }
            break;
        }
    }
    this->tokens.expectSemicolon("error_page");
}

void ConfigParser::serverLocation(ServerConfig &conf)
{
    this->tokens.expect("Expected location");

    Location loc;
    this->parseLocationBlock(loc); 
    for (size_t i = 0; i < conf.locations.size(); ++i)
    {
        if (conf.locations[i].path == loc.path)
            throwError(ERR_DUPLICATE_VALUE, loc.path);
    }
    conf.locations.push_back(loc);
}
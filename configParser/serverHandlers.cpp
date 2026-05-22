#include "configParser.hpp"

void ConfigParser::serverListen(ServerConfig &conf)
{
    if (conf.seenDirectives["listen"])
        throw std::runtime_error("duplicate listen directive");

    conf.seenDirectives["listen"] = true;
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
        throw std::runtime_error("Invalid listen syntax (missing IP before colon): " + value);
    if (colonPos == value.length() - 1)
        throw std::runtime_error("Invalid listen syntax (missing port after colon): " + value);
    
    std::string ipPart = value.substr(0, colonPos);
    if (ipPart == "localhost")
        ipPart = "127.0.0.1";
        
    if (!isValidHost(ipPart))
        throw std::runtime_error("Invalid host: " + ipPart);
        
    currListen.host = ipPart;

    std::string portPart = value.substr(colonPos + 1);
    currListen.port = valuesParser::parsePortValue(portPart); 
    conf.listens.push_back(currListen);

    this->tokens.expectSemicolon("listen");
}

void ConfigParser::serverRoot(ServerConfig &conf)
{
    if (conf.seenDirectives["root"])
        throw std::runtime_error("duplicate root directive");

    conf.seenDirectives["root"] = true;
    
    this->tokens.expect("Expected root");

    conf.root = valuesParser::parseFilesystemPath(this->tokens);
    this->tokens.expectSemicolon("root");
}

void ConfigParser::serverNames(ServerConfig &conf)
{
    if (conf.seenDirectives["server_name"])
        throw std::runtime_error("duplicate server_name directive");

    conf.seenDirectives["server_name"] = true;
    this->tokens.expect("Expected server_name");
    conf.serverName = valuesParser::parseWordListUntilSemicolon(this->tokens, "server_name");
}

void ConfigParser::serverIndex(ServerConfig &conf)
{
    if (conf.seenDirectives["index"])
        throw std::runtime_error("duplicate index directive");

    conf.seenDirectives["index"] = true;    
    
    this->tokens.expect("Expected index");

    conf.indexes = valuesParser::parseIndexesList(this->tokens);
}

void ConfigParser::serverClientMaxBodySize(ServerConfig &conf)
{
    if (conf.seenDirectives["client_max_body_size"])
        throw std::runtime_error("duplicate client_max_body_size directive");

    conf.seenDirectives["client_max_body_size"] = true;
    
    this->tokens.expect("Expected client_max_body_size");

    conf.client_max_body_size = valuesParser::parseBodySizeValue(this->tokens);
    
    if (conf.client_max_body_size == 0)
        throw std::runtime_error("Invalid client_max_body_size");
        
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
                throw std::runtime_error("Invalid error_page status code (overflow): " + curr);

            if (!isValidErrorCode(errorCode))
                throw std::runtime_error("Unsupported or invalid HTTP error status code: " + curr);

            codes.push_back(errorCode);
            this->tokens.expect("Expected error code"); 
        }
        else
        {
            if (codes.empty())
                throw std::runtime_error("Invalid error_page syntax: requires at least one status code");

            std::string path = valuesParser::parseErrorPagePathValue(this->tokens);

            for (size_t i = 0; i < codes.size(); ++i)
            {
                if (conf.errorPage.count(codes[i]))
                    throw std::runtime_error("Duplicate error_page declaration for status code");

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

    for (size_t i = 0; i < conf.Locations.size(); ++i)
    {
        if (conf.Locations[i].path == loc.path)
            throw std::runtime_error("Duplicate location: " + loc.path);
    }
    conf.Locations.push_back(loc);
}
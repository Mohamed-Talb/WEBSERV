#ifndef CONFIG_ENTITIES_HPP
#define CONFIG_ENTITIES_HPP

#include <map>
#include <string>
#include <vector>
#include <limits>  
#include <sstream>
#include <cstdlib>
#include <stdexcept>
#include <algorithm>
#include <sys/types.h>
#include "../Helpers.hpp"
#include "tokenStream.hpp"
#include "valuesParser.hpp"

struct Listen
{
    int port;
    std::string host;
    Listen() : port(80), host("127.0.0.1"){};
};

struct Location 
{
    std::string path;
    std::string root;
    std::string cgiExt;
    std::string cgiPath;
    
    std::string autoindex;
    
    std::string uploadPath;
    std::string uploadEnabled;
    
    int redirectCode;
    std::string redirectTarget;

    std::vector<std::string> methods;
    std::vector<std::string> indexes;
    std::vector<std::string> allowedMethods;
    std::map<std::string, bool> seenDirectives;
    
    Location() : autoindex("off"), uploadEnabled("off") ,redirectCode(0)
    {
        allowedMethods.push_back("GET");
        allowedMethods.push_back("DELETE");
        allowedMethods.push_back("POST");
    };
};

struct ServerConfig 
{
    int port; // !!
    std::string host;// !! 
    // ..... I will leave this for now to avoid a server error
    
    std::string root;
    std::vector<Listen> listens;
    ssize_t client_max_body_size;
    std::vector<Location> locations;
    std::vector<std::string> indexes;
    std::vector<std::string> serverNames;
    std::map<int, std::string> errorPage;
    std::map<std::string, bool> seenDirectives;
    ServerConfig() : root("./www"), client_max_body_size(1048576)
    {
        indexes.push_back("index.html");
        serverNames.push_back(""); 
    }
};

struct CompareLocations
{
    bool operator()(const Location &a, const Location &b) const 
    {
        return a.path.size() > b.path.size();
    }
};

class ConfigParser
{

    private:
    TokenStream tokens;

    void checkDuplicate(Location &loc, const std::string &directive) const;
    void checkDuplicate(ServerConfig &conf, const std::string &directive) const;

    typedef void (ConfigParser::*ServerHandler)(ServerConfig &);
    typedef void (ConfigParser::*LocationHandler)(Location &);

    std::map<std::string, ServerHandler> serverDispatch;
    std::map<std::string, LocationHandler> locationDispatch;

    // Initialization
    void initServerDispatch();
    void initLocationDispatch();

    // Main parse flow
    void parseLocationBlock(Location &loc);
    void parseServerBlock(ServerConfig &conf);

    //
    void validateServer(ServerConfig &conf);
    void validateLocation(Location &loc);
    // Server directive handlers
    void serverRoot(ServerConfig &conf);
    void serverIndex(ServerConfig &conf);
    void serverListen(ServerConfig &conf);
    void serverLocation(ServerConfig &conf);
    void serverErrorPages(ServerConfig &conf);
    void serverNames(ServerConfig &conf);
    void serverClientMaxBodySize(ServerConfig &conf);

    // Location directive handlers
    void locationRoot(Location &loc);
    void locationIndex(Location &loc);
    void locationCgiExt(Location &loc);
    void locationUpload(Location &loc);
    void locationCgiPath(Location &loc);
    void locationMethods(Location &loc);
    void locationRedirect(Location &loc);
    void locationAutoindex(Location &loc);
    void locationUploadPath(Location &loc);

    public:
    ConfigParser();
    std::vector<ServerConfig> loadeConfig(std::string configFile);
};


#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>

class TokenStream
{
    private:
    std::vector<std::string> tokens;
    std::vector<std::string>::const_iterator it;

    public:
    TokenStream(std::string tokens);
    TokenStream();

    bool hasMore() const;
    const std::string &current() const;
    std::string expect(const std::string &err);
    TokenStream &operator=(const TokenStream &other);
    void expectSemicolon(const std::string &directive);
};


#include <map>
#include <string>
#include <vector>
#include <limits>  
#include <sstream>
#include <cstdlib>
#include <algorithm>
#include <stdexcept>
#include <sys/types.h>
#include "../Helpers.hpp"
#include "tokenStream.hpp"

namespace valuesParser
{
    int parsePortValue(std::string value);
    size_t parseBodySizeValue(TokenStream &tokens);
    std::string parseCgiExtValue(TokenStream &tokens);
    std::string parseCgiPathValue(TokenStream &tokens);
    std::string parseLocationPath(TokenStream &tokens);
    std::string parseFilesystemPath(TokenStream &tokens);
    std::string parseErrorPagePathValue(TokenStream &tokens);
    std::string parseRedirectTargetValue(TokenStream &tokens);
    std::vector<std::string> parseIndexesList(TokenStream &tokens);
    std::vector<std::string> parseWordListUntilSemicolon(TokenStream &tokens, const std::string &directiveName);
}

#include "tokenStream.hpp"

std::vector<std::string> fileToTokens(const std::string &filepath)
{
    std::vector<std::string> tokens;
    std::ifstream file(filepath.c_str());
    std::string specials = "{;}";

    if (!file.is_open())
        throw std::runtime_error("Could not open config file: " + filepath);

    std::string line;
    while (std::getline(file, line))
    {
        size_t commentPos = line.find('#');

        if (commentPos != std::string::npos)
            line.erase(commentPos);

        std::string processedLine;

        for (size_t i = 0; i < line.size(); ++i)
        {
            if (specials.find(line[i]) != std::string::npos)
            {
                processedLine += ' ';
                processedLine += line[i];
                processedLine += ' ';
            }
            else
            {
                processedLine += line[i];
            }
        }
        std::stringstream ss(processedLine);
        std::string token;

        while (ss >> token)
            tokens.push_back(token);
    }
    return tokens;
}


TokenStream::TokenStream(std::string filePath)
{
    tokens = fileToTokens(filePath);
    it = tokens.begin();
}

TokenStream::TokenStream() {}

TokenStream &TokenStream::operator=(const TokenStream &other)
{
    if (this != &other) 
    {
        size_t offset = std::distance(other.tokens.begin(), other.it);
        this->tokens = other.tokens;
        this->it = this->tokens.begin() + offset;
    }
    return *this;
}

bool TokenStream::hasMore() const
{
    return it != tokens.end();
}

const std::string &TokenStream::current() const
{
    if (it == tokens.end())
        throw std::runtime_error("Unexpected end of config");

    return *it;
}

std::string TokenStream::expect(const std::string &err)
{
    if (it == tokens.end())
        throw std::runtime_error(err);

    std::string value = *it;
    ++it;
    return value;
}

void TokenStream::expectSemicolon(const std::string &directive)
{
    if (it == tokens.end() || *it != ";")
        throw std::runtime_error("Missing ';' after " + directive);
    ++it;
}


#include "valuesParser.hpp"
#include "../Helpers.hpp"
#include <cstdlib>
#include <sstream>
#include <stdexcept>

/*
===============================================================================
 PATH HANDLING RULES (CONFIG PARSER + HTTP RESOLUTION)
===============================================================================

We follow Postel’s Law:
→ Accept flexible input
→ Normalize internally for consistency and safety

------------------------------------------------------------------------------
| ELEMENT          | ACCEPTED INPUT              | STORED FORMAT              |
------------------------------------------------------------------------------
| root             | "./www", "./www/"           | "./www"                    |
| location.path    | "/", "/img", "/img/"        | "/", "/img"                |
| index            | "index.html", "/index.html" | "index.html"               |
| cgi_path         | "./cgi-bin", "./cgi-bin/"   | "./cgi-bin"                |
| cgi_ext          | ".py", "py"                 | ".py"                      |
| error_page path  | "404.html", "/404.html"     | "/404.html"                |
------------------------------------------------------------------------------

------------------------------------------------------------------------------
 NORMALIZATION RULES
------------------------------------------------------------------------------

1. ROOT
   - Remove trailing slashes
   - Example:
        "./www/" → "./www"

2. LOCATION PATH
   - Must start with '/'
   - Remove trailing slash (except "/")
   - Example:
        "/images/" → "/images"

3. INDEX
   - Remove leading slashes
   - Must be relative to root
   - Reject ".." (security)
   - Example:
        "/index.html" → "index.html"

4. CGI EXT
   - Must start with '.'
   - Example:
        "py" → ".py"

5. ERROR PAGE PATH
   - Must start with '/'
   - Reject ".."
   - Example:
        "404.html" → "/404.html"

------------------------------------------------------------------------------
 SECURITY RULES
------------------------------------------------------------------------------

- Reject any path containing ".." (directory traversal)
- Never allow escaping the root directory

Multiple slashes should be collapsed:

    "///img//cat.png" → "/img/cat.png"
*/
#include "../Errors.hpp"

namespace valuesParser
{

int parsePortValue(std::string value)
{
    if (!isOnlyDigits(value))
        throwError(ERR_INVALID_VALUE, value + " (must be numeric)");

    if (value.size() > 5)
        throwError(ERR_INVALID_VALUE, value + " (too long)");

    int port = std::atoi(value.c_str());

    if (port <= 0 || port > 65535)
        throwError(ERR_INVALID_VALUE, value + " (must be between 1 and 65535)");

    return port;
}

size_t parseBodySizeValue(TokenStream &tokens)
{
    std::string value = tokens.expect("client_max_body_size value"); // The expect string gets caught by EOF handler if missing

    if (value.empty())
        throwError(ERR_MISSING_VALUE, "client_max_body_size");

    size_t i = 0;
    while (i < value.size() && std::isdigit(static_cast<unsigned char>(value[i])))
    {
        i++;
    }
    if (i < value.size() && value[i] == '.')
    {
        throwError(ERR_INVALID_VALUE, value + " (decimals are not allowed)");
    }
    if (i == 0)
        throwError(ERR_INVALID_VALUE, value + " (missing digits)");

    std::string numericPart = value.substr(0, i);
    std::string unitPart = value.substr(i);

    size_t baseSize = 0;
    std::istringstream iss(numericPart);
    iss >> baseSize;

    if (iss.fail())
        throwError(ERR_INVALID_VALUE, numericPart + " (numeric overflow)");

    std::string unit = toUpper(trim(unitPart)); 
    size_t multiplier = 1;

    if (unit.empty() || unit == "B") 
    {
        multiplier = 1;
    }
    else if (unit == "K" || unit == "KB")
    {
        multiplier = 1024;
    }
    else if (unit == "M" || unit == "MB")
    {
        multiplier = 1024 * 1024;
    } 
    else if (unit == "G" || unit == "GB")
    {
        multiplier = 1024 * 1024 * 1024;
    }
    else
    {
        throwError(ERR_INVALID_VALUE, unitPart + " (unsupported byte size unit suffix)");
    }
    if (baseSize > 0 && multiplier > std::numeric_limits<size_t>::max() / baseSize)
    {
        throwError(ERR_INVALID_VALUE, value + " (exceeds physical address space limits)");
    }
    return baseSize * multiplier;
}

std::string parseErrorPagePathValue(TokenStream &tokens)
{
    std::string raw = tokens.expect("error_page path");
    std::string path = mergeSlashes(raw);

    if (path.empty())
        throwError(ERR_MISSING_VALUE, "error_page path");

    if (path.find("..") != std::string::npos)
        throwError(ERR_INVALID_VALUE, path + " (directory traversal not allowed)");

    if (path[0] != '/')
        path = "/" + path;

    return path;
}

std::string parseCgiExtValue(TokenStream &tokens)
{
    std::string raw = tokens.expect("cgi_ext");
    std::string ext = raw;

    if (ext.empty())
        throwError(ERR_MISSING_VALUE, "cgi_ext");

    if (ext.find("..") != std::string::npos || ext.find("/") != std::string::npos)
        throwError(ERR_INVALID_VALUE, ext + " (invalid characters in extension)");

    if (ext[0] != '.')
        ext = "." + ext;

    if (ext.size() == 1)
        throwError(ERR_INVALID_VALUE, ext + " (extension cannot be just '.')");

    return ext;
}

std::string parseRedirectTargetValue(TokenStream &tokens)
{
    std::string target = tokens.expect("redirect target");

    if (target.empty())
        throwError(ERR_MISSING_VALUE, "redirect target");

    if (target.find("..") != std::string::npos)
        throwError(ERR_INVALID_VALUE, target + " (directory traversal not allowed)");

    if (target[0] != '/' && target.find("http://") != 0  &&
        target.find("https://") != 0)
        throwError(ERR_INVALID_VALUE, target + " (must be absolute path or URL)");

    return target;
}

std::string parseFilesystemPath(TokenStream &tokens)
{
    std::string root = tokens.expect("path");

    root = mergeSlashes(root);

    while (root.size() > 1 && root[root.size() - 1] == '/')
        root.erase(root.size() - 1);

    if (root.empty())
        throwError(ERR_INVALID_VALUE, root + " (empty path)");

    if (root.find("..") != std::string::npos)
        throwError(ERR_INVALID_VALUE, root + " (directory traversal not allowed)");

    return root;
}

std::string parseLocationPath(TokenStream &tokens)
{
    std::string path = tokens.expect("location path");

    path = mergeSlashes(path);

    if (path.empty() || path[0] != '/')
        throwError(ERR_INVALID_VALUE, path + " (must start with '/')");

    while (path.size() > 1 && path[path.size() - 1] == '/')
        path.erase(path.size() - 1);

    if (path.find("..") != std::string::npos)
        throwError(ERR_INVALID_VALUE, path + " (directory traversal not allowed)");

    return path;
}

std::vector<std::string> parseWordListUntilSemicolon(TokenStream &tokens, const std::string &directiveName)
{
    std::vector<std::string> values;

    while (tokens.hasMore() && tokens.current() != ";")
    {
        values.push_back(tokens.expect(directiveName + " value"));
    }
    
    if (!tokens.hasMore())
        throwError(ERR_MISSING_SEMICOLON, directiveName);

    if (values.empty())
        throwError(ERR_MISSING_VALUE, directiveName + " (requires at least one value)");
    return values;
}

std::vector<std::string> parseIndexesList(TokenStream &tokens)
{
    std::vector<std::string> indexes = parseWordListUntilSemicolon(tokens, "index");

    for (size_t i = 0; i < indexes.size(); ++i)
    {
        indexes[i] = mergeSlashes(indexes[i]);

        while (!indexes[i].empty() && indexes[i][0] == '/')
            indexes[i].erase(0, 1);

        if (indexes[i].empty())
            throwError(ERR_INVALID_VALUE, "Empty index name");

        if (indexes[i].find("..") != std::string::npos)
            throwError(ERR_INVALID_VALUE, indexes[i] + " (directory traversal not allowed)");
    }
    return indexes;
}

std::string parseCgiPathValue(TokenStream &tokens)
{
    std::string path = tokens.expect("cgi_path");

    path = mergeSlashes(path);

    while (path.size() > 1 && path[path.size() - 1] == '/')
        path.erase(path.size() - 1);

    if (path.empty())
        throwError(ERR_INVALID_VALUE, "Empty cgi_path");

    if (path.find("..") != std::string::npos)
        throwError(ERR_INVALID_VALUE, path + " (directory traversal not allowed)");
    return path;
}

}


#include "configParser.hpp"
#include "../Errors.hpp"

void ConfigParser::locationMethods(Location &loc)
{
    this->checkDuplicate(loc, "methods");
    tokens.expect("Expected methods");
    std::vector<std::string> methods = valuesParser::parseWordListUntilSemicolon(tokens, "methods");
    for (size_t i = 0; i < methods.size(); ++i)
    {
        std::string currMethod = toUpper(methods[i]);
        if (std::find(loc.allowedMethods.begin(), loc.allowedMethods.end(), currMethod) == loc.allowedMethods.end())
        {
            throwError(ERR_INVALID_VALUE, currMethod);
        }
        if (std::find(loc.methods.begin(), loc.methods.end(), currMethod) != loc.methods.end())
        {
            throwError(ERR_DUPLICATE_VALUE, currMethod);
        }
        loc.methods.push_back(currMethod);
    }
    tokens.expectSemicolon("methods");
}

void ConfigParser::locationRoot(Location &loc)
{
    this->checkDuplicate(loc, "root");
    
    tokens.expect("Expected root");
    loc.root = valuesParser::parseFilesystemPath(tokens);
    tokens.expectSemicolon("root");
}

void ConfigParser::locationAutoindex(Location &loc)
{
    this->checkDuplicate(loc, "autoindex");
    tokens.expect("Expected autoindex");
    loc.autoindex = tokens.expect("Missing autoindex value");
    if (loc.autoindex != "on" && loc.autoindex != "off")
        throwError(ERR_INVALID_VALUE, loc.autoindex);

    tokens.expectSemicolon("autoindex");
}

void ConfigParser::locationIndex(Location &loc)
{
    this->checkDuplicate(loc, "index");
    
    tokens.expect("Expected index");
    loc.indexes = valuesParser::parseIndexesList(tokens);
    tokens.expectSemicolon("index"); 
}

void ConfigParser::locationCgiPath(Location &loc)
{
    this->checkDuplicate(loc, "cgi_path");

    tokens.expect("Expected cgi_path");
    loc.cgiPath = valuesParser::parseCgiPathValue(tokens);
    tokens.expectSemicolon("cgi_path");
}

void ConfigParser::locationCgiExt(Location &loc)
{
    this->checkDuplicate(loc, "cgi_ext");
    
    tokens.expect("Expected cgi_ext");
    loc.cgiExt = valuesParser::parseCgiExtValue(tokens);
    tokens.expectSemicolon("cgi_ext");
}

void ConfigParser::locationRedirect(Location &loc)
{
    this->checkDuplicate(loc, "redirect");

    tokens.expect("Expected redirect");

    std::string codeValue = tokens.expect("Missing redirect code");

    if (!isOnlyDigits(codeValue))
        throwError(ERR_INVALID_VALUE, codeValue);

    loc.redirectCode = std::atoi(codeValue.c_str());

    if (loc.redirectCode != 301 && loc.redirectCode != 302)
        throwError(ERR_INVALID_VALUE, codeValue);

    loc.redirectTarget = valuesParser::parseRedirectTargetValue(tokens);

    tokens.expectSemicolon("redirect");
}

void ConfigParser::locationUpload(Location &loc)
{
    this->checkDuplicate(loc, "upload");
    
    tokens.expect("Expected upload");

    loc.uploadEnabled = tokens.expect("Missing upload value");

    if (loc.uploadEnabled != "on" && loc.uploadEnabled != "off")
        throwError(ERR_INVALID_VALUE, loc.uploadEnabled);

    tokens.expectSemicolon("upload");
}

void ConfigParser::locationUploadPath(Location &loc)
{
    this->checkDuplicate(loc, "upload_path");
    tokens.expect("Expected upload_path");
    loc.uploadPath = valuesParser::parseFilesystemPath(tokens);
    tokens.expectSemicolon("upload_path");
}


#include "configParser.hpp"
#include "../Errors.hpp"

void ConfigParser::serverListen(ServerConfig &conf)
{
    if (conf.seenDirectives["listen"])
        throw std::runtime_error("Error: Only one listen directive allowed per server block");

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

#include "configParser.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cstdlib>
#include <algorithm>
#include "../Helpers.hpp"

void ConfigParser::validateLocation(Location &loc)
{
    if (loc.uploadEnabled == "on" && loc.uploadPath.empty())
        throw std::runtime_error("upload on requires upload_path");

    if (loc.uploadEnabled == "off" && !loc.uploadPath.empty())
        throw std::runtime_error("upload_path set but upload is off");

    if (!loc.cgiExt.empty() && loc.cgiPath.empty())
        throw std::runtime_error("cgi_ext requires cgi_path");

    if (!loc.cgiPath.empty() && loc.cgiExt.empty())
        throw std::runtime_error("cgi_path requires cgi_ext");

    if (loc.autoindex != "on" && loc.autoindex != "off")
        throw std::runtime_error("autoindex must be 'on' or 'off'");

    if (loc.redirectCode != 0)
    {
        if (loc.redirectCode != 301 && loc.redirectCode != 302)
            throw std::runtime_error("Invalid redirect code");

        if (loc.redirectTarget.empty())
            throw std::runtime_error("Missing redirect target");

        if (loc.redirectTarget.find("..") != std::string::npos)
            throw std::runtime_error("Invalid redirect target");

        if (loc.redirectTarget[0] != '/' && loc.redirectTarget.find("http://") != 0 && loc.redirectTarget.find("https://") != 0)
        {
            throw std::runtime_error("redirect target must be path or URL");
        }
    }
}

void ConfigParser::checkDuplicate(ServerConfig &conf, const std::string &directive) const
{
    if (conf.seenDirectives[directive])
        throw std::runtime_error("duplicate " + directive + " directive in server block");
    
    conf.seenDirectives[directive] = true;
}

void ConfigParser::checkDuplicate(Location &loc, const std::string &directive) const
{
    if (loc.seenDirectives[directive])
        throw std::runtime_error("duplicate " + directive + " directive in location block");
    
    loc.seenDirectives[directive] = true;
}

void ConfigParser::validateServer(ServerConfig &conf) 
{
    if (conf.listens.empty())
    {
        conf.listens.push_back(Listen());
    }
    conf.host = conf.listens[0].host;
    conf.port = conf.listens[0].port;
    for (size_t i = 0; i < conf.locations.size(); ++i)
    {
        Location &loc = conf.locations[i];
        if (loc.root.empty()) 
        {
            loc.root = conf.root;
        }
        if (loc.indexes.empty()) 
        {
            loc.indexes = conf.indexes;
        }
        validateLocation(loc);
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

    loc.path = valuesParser::parseLocationPath(this->tokens);

    if (tokens.expect("Expected '{'") != "{")
        throw std::runtime_error("Expected '{'");
    while (tokens.hasMore())
    {
        if (tokens.current() == "}")
        {
            tokens.expect("Expected '}'");
            validateLocation(loc);
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
            std::sort(conf.locations.begin(), conf.locations.end(), CompareLocations());
            validateServer(conf);
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

std::vector<ServerConfig> ConfigParser::loadeConfig(std::string configFile)
{
    
    tokens = TokenStream(configFile);
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
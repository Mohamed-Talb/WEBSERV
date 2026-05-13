#include "config.hpp"

#include "configParser.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>

std::vector<std::string> tokenize(const std::string &filepath)
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

Location::Location() : redirectCode(0), autoindex("off"), uploadEnabled("off") 
{
    allowedMethods.push_back("GET");
    allowedMethods.push_back("DELETE");
    allowedMethods.push_back("POST");
};

void Location::validateLocation() const 
{
    if (uploadEnabled == "on" && uploadPath.empty())
        throw std::runtime_error("upload on requires upload_path");

    if (uploadEnabled == "off" && !uploadPath.empty())
        throw std::runtime_error("upload_path set but upload is off");

    if (!cgiExt.empty() && cgiPath.empty())
        throw std::runtime_error("cgi_ext requires cgi_path");

    if (!cgiPath.empty() && cgiExt.empty())
        throw std::runtime_error("cgi_path requires cgi_ext");

    if (autoindex != "on" && autoindex != "off")
        throw std::runtime_error("autoindex must be 'on' or 'off'");

    if (redirectCode != 0)
    {
        if (redirectCode != 301 && redirectCode != 302)
            throw std::runtime_error("Invalid redirect code");

        if (redirectTarget.empty())
            throw std::runtime_error("Missing redirect target");

        if (redirectTarget.find("..") != std::string::npos)
            throw std::runtime_error("Invalid redirect target");

        if (redirectTarget[0] != '/' && redirectTarget.find("http://") != 0 && redirectTarget.find("https://") != 0)
        {
            throw std::runtime_error("redirect target must be path or URL");
        }
    }
}

ServerConfig::ServerConfig() : port(80),host("127.0.0.1"),root("./www"),client_max_body_size(1048576) 
{}

void ServerConfig::validate() const 
{
    for (size_t i = 0; i < Locations.size(); ++i)
        Locations[i].validateLocation();
}


#include "configParser.hpp"

void ConfigParser::handleLocMethods(Location &loc)
{
    if (loc.seenDirectives["methods"])
        throw std::runtime_error("duplicate methods directive");

    loc.seenDirectives["methods"] = true;

    tokens.expect("Expected methods");

    std::vector<std::string> methods = parseWordListUntilSemicolon("methods");
    for (size_t i = 0; i < methods.size(); ++i)
    {
        std::string currMethod = toUpper(methods[i]);
        if (std::find(loc.allowedMethods.begin(),loc.allowedMethods.end(),currMethod) == loc.allowedMethods.end())
        {
            throw std::runtime_error("unsupported method: " + currMethod);
        }
        if (std::find(loc.methods.begin(),loc.methods.end(),currMethod) != loc.methods.end())
        {
            throw std::runtime_error("duplicate method: " + currMethod);
        }
        loc.methods.push_back(currMethod);
    }
}

void ConfigParser::handleLocRoot(Location &loc)
{
    if (loc.seenDirectives["root"])
        throw std::runtime_error("duplicate root directive in location");

    loc.seenDirectives["root"] = true;
    tokens.expect("Expected root");

    loc.root = parseFilesystemPath();

    tokens.expectSemicolon("root");
}

void ConfigParser::handleLocAutoindex(Location &loc)
{
    if (loc.seenDirectives["autoindex"])
        throw std::runtime_error("duplicate autoindex directive");

    loc.seenDirectives["autoindex"] = true;
    tokens.expect("Expected autoindex");

    loc.autoindex = tokens.expect("Missing autoindex value");

    if (loc.autoindex != "on" && loc.autoindex != "off")
        throw std::runtime_error("autoindex must be 'on' or 'off'");

    tokens.expectSemicolon("autoindex");
}

void ConfigParser::handleLocIndex(Location &loc)
{
    if (loc.seenDirectives["index"])
        throw std::runtime_error("duplicate index directive in location");

    loc.seenDirectives["index"] = true;
    tokens.expect("Expected index");
    loc.indexes = parseIndexesList();
}

void ConfigParser::handleLocCgiPath(Location &loc)
{
    if (loc.seenDirectives["cgi_path"])
        throw std::runtime_error("duplicate cgi_path directive");

    loc.seenDirectives["cgi_path"] = true;

    tokens.expect("Expected cgi_path");

    loc.cgiPath = parseCgiPathValue();

    tokens.expectSemicolon("cgi_path");
}

void ConfigParser::handleLocCgiExt(Location &loc)
{
    if (loc.seenDirectives["cgi_ext"])
        throw std::runtime_error("duplicate cgi_ext directive");

    loc.seenDirectives["cgi_ext"] = true;
    tokens.expect("Expected cgi_ext");

    std::string value = tokens.expect("Missing cgi_ext");
    loc.cgiExt = parseCgiExtValue(value);

    tokens.expectSemicolon("cgi_ext");
}

void ConfigParser::handleLocRedirect(Location &loc)
{
    if (loc.seenDirectives["redirect"])
        throw std::runtime_error("duplicate redirect directive");

    loc.seenDirectives["redirect"] = true;
    if (loc.redirectCode != 0)
        throw std::runtime_error("duplicate redirect directive");

    tokens.expect("Expected redirect");

    std::string codeValue = tokens.expect("Missing redirect code");

    if (!isOnlyDigits(codeValue))
        throw std::runtime_error("Invalid redirect code");

    loc.redirectCode = std::atoi(codeValue.c_str());

    if (loc.redirectCode != 301 && loc.redirectCode != 302)
        throw std::runtime_error("Invalid redirect code");

    loc.redirectTarget = parseRedirectTargetValue(
        tokens.expect("Missing redirect target")
    );

    tokens.expectSemicolon("redirect");
}

void ConfigParser::handleLocUpload(Location &loc)
{
    if (loc.seenDirectives["upload"])
        throw std::runtime_error("duplicate upload directive");

    loc.seenDirectives["upload"] = true;
    tokens.expect("Expected upload");

    loc.uploadEnabled = tokens.expect("Missing upload value");

    if (loc.uploadEnabled != "on" && loc.uploadEnabled != "off")
        throw std::runtime_error("upload must be 'on' or 'off'");

    tokens.expectSemicolon("upload");
}

void ConfigParser::handleLocUploadPath(Location &loc)
{
    if (loc.seenDirectives["upload_path"])
        throw std::runtime_error("duplicate upload_path directive");

    loc.seenDirectives["upload_path"] = true;
    if (!loc.uploadPath.empty())
        throw std::runtime_error("duplicate upload_path directive");

    tokens.expect("Expected upload_path");

    loc.uploadPath = parseFilesystemPath();

    tokens.expectSemicolon("upload_path");
}


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


#include "configParser.hpp"

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

int ConfigParser::parsePortValue(const std::string &value)
{
    if (!isOnlyDigits(value))
        throw std::runtime_error("Invalid port: " + value);

    if (value.size() > 5)
        throw std::runtime_error("Invalid port: " + value);

    int port = std::atoi(value.c_str());

    if (port <= 0 || port > 65535)
        throw std::runtime_error("Invalid port: " + value);

    return port;
}

size_t ConfigParser::parseBodySizeValue(const std::string &value)
{
    if (!isOnlyDigits(value))
        throw std::runtime_error("Invalid client_max_body_size value: " + value);

    return static_cast<size_t>(std::atol(value.c_str()));
}

std::string ConfigParser::parseErrorPagePathValue(const std::string &raw)
{
    std::string path = mergeSlashes(raw);

    if (path.empty())
        throw std::runtime_error("Invalid error_page path");

    if (path.find("..") != std::string::npos)
        throw std::runtime_error("Invalid error_page path");

    if (path[0] != '/')
        path = "/" + path;

    return path;
}

std::string ConfigParser::parseCgiExtValue(const std::string &raw)
{
    std::string ext = raw;

    if (ext.empty())
        throw std::runtime_error("Invalid cgi_ext");

    if (ext.find("..") != std::string::npos || ext.find("/") != std::string::npos)
        throw std::runtime_error("Invalid cgi_ext");

    if (ext[0] != '.')
        ext = "." + ext;

    if (ext.size() == 1)
        throw std::runtime_error("Invalid cgi_ext");

    return ext;
}

std::string ConfigParser::parseRedirectTargetValue(const std::string &target)
{
    if (target.empty())
        throw std::runtime_error("Missing redirect target");

    if (target.find("..") != std::string::npos)
        throw std::runtime_error("Invalid redirect target");

    if (target[0] != '/' && target.find("http://") != 0  &&
        target.find("https://") != 0)
        throw std::runtime_error("redirect target must be path or URL");

    return target;
}

std::string ConfigParser::parseFilesystemPath()
{
    std::string root = tokens.expect("Missing root");

    root = mergeSlashes(root);

    while (root.size() > 1 && root[root.size() - 1] == '/')
        root.erase(root.size() - 1);

    if (root.empty())
        throw std::runtime_error("Invalid root path");

    if (root.find("..") != std::string::npos)
        throw std::runtime_error("Invalid root path: directory traversal");

    return root;
}

std::string ConfigParser::parseLocationPath()
{
    std::string path = tokens.expect("Missing location path");

    path = mergeSlashes(path);

    if (path.empty() || path[0] != '/')
        throw std::runtime_error("Location path must start with '/'");

    while (path.size() > 1 && path[path.size() - 1] == '/')
        path.erase(path.size() - 1);

    if (path.find("..") != std::string::npos)
        throw std::runtime_error("Invalid location path: directory traversal");

    return path;
}


std::vector<std::string> ConfigParser::parseWordListUntilSemicolon(const std::string &directiveName)
{
    std::vector<std::string> values;

    while (tokens.hasMore() && tokens.current() != ";")
    {
        values.push_back(tokens.expect("Missing value for " + directiveName));
    }
    if (!tokens.hasMore())
        throw std::runtime_error("Missing ';' after " + directiveName);

    if (values.empty())
        throw std::runtime_error(directiveName + " directive requires at least one value");
    tokens.expectSemicolon(directiveName);
    return values;
}


std::vector<std::string> ConfigParser::parseIndexesList()
{
    std::vector<std::string> indexes = parseWordListUntilSemicolon("index");

    for (size_t i = 0; i < indexes.size(); ++i)
    {
        indexes[i] = mergeSlashes(indexes[i]);

        while (!indexes[i].empty() && indexes[i][0] == '/')
            indexes[i].erase(0, 1);

        if (indexes[i].empty())
            throw std::runtime_error("Index cannot be empty");

        if (indexes[i].find("..") != std::string::npos)
            throw std::runtime_error("Invalid index path: directory traversal");
    }

    return indexes;
}

std::string ConfigParser::parseCgiPathValue()
{
    std::string path = tokens.expect("Missing cgi_path");

    path = mergeSlashes(path);

    while (path.size() > 1 && path[path.size() - 1] == '/')
        path.erase(path.size() - 1);

    if (path.empty())
        throw std::runtime_error("Invalid cgi_path");

    if (path.find("..") != std::string::npos)
        throw std::runtime_error("Invalid cgi_path: directory traversal");
    return path;
}

#include "configParser.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cstdlib>
#include <algorithm>
#include "../Helpers.hpp"

ConfigParser::ConfigParser(const std::vector<std::string> &tokens) : tokens(tokens)
{
    initServerDispatch();
    initLocationDispatch();
}

void ConfigParser::initServerDispatch()
{
    serverDispatch["host"] = &ConfigParser::handleHost;
    serverDispatch["listen"] = &ConfigParser::handleListen;
    serverDispatch["root"] = &ConfigParser::handleRoot;
    serverDispatch["server_name"] = &ConfigParser::handleServerName;
    serverDispatch["location"] = &ConfigParser::handleLocation;
    serverDispatch["index"] = &ConfigParser::handleIndex;
    serverDispatch["error_page"] = &ConfigParser::handleErrorPage;
    serverDispatch["client_max_body_size"] = &ConfigParser::handleClientMaxBodySize;
}

void ConfigParser::initLocationDispatch()
{
    locationDispatch["methods"] = &ConfigParser::handleLocMethods;
    locationDispatch["root"] = &ConfigParser::handleLocRoot;
    locationDispatch["autoindex"] = &ConfigParser::handleLocAutoindex;
    locationDispatch["index"] = &ConfigParser::handleLocIndex;
    locationDispatch["cgi_path"] = &ConfigParser::handleLocCgiPath;
    locationDispatch["cgi_ext"] = &ConfigParser::handleLocCgiExt;
    locationDispatch["return"] = &ConfigParser::handleLocRedirect;
    locationDispatch["upload"] = &ConfigParser::handleLocUpload;
    locationDispatch["upload_path"] = &ConfigParser::handleLocUploadPath;
}

 
void ConfigParser::parseLocationBlock(Location &loc)
{
    loc.path = parseLocationPath();

    if (tokens.expect("Expected '{'") != "{")
        throw std::runtime_error("Expected '{'");

    while (tokens.hasMore())
    {
        if (tokens.current() == "}")
        {
            tokens.expect("Expected '}'");
            loc.validateLocation();
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
            std ::sort(conf.Locations.begin(), conf.Locations.end(), CompareLocations());
            conf.validate();
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

std::vector<ServerConfig> ConfigParser::parse()
{
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
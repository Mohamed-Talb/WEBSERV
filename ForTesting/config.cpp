
#include <vector>
#include <string>
#include <map>
#include <unistd.h>
#include <stdexcept>
#include <iostream>
#include <stdlib.h>
#include <fstream>
#include <sstream>
#include "../Helpers.hpp"
#include <algorithm>


typedef std::vector<std::string> Tokens;
typedef Tokens::iterator TokenIt;

std::string parseErrorPagePath(const std::string &raw);
int         parsePort(TokenIt &it, const Tokens &tokens);
std::string parseRoot(TokenIt &it, const Tokens &tokens);
std::vector<std::string> parseIndexes(TokenIt &it, const Tokens &tokens);
std::string parseCgiExt(TokenIt &it, const Tokens &tokens);
size_t      parseBodySize(TokenIt &it, const Tokens &tokens);
std::string parseLocationPath(TokenIt &it, const Tokens &tokens);
std::string expect(TokenIt &it, const Tokens &tokens, const std::string &err);
void        expectSemicolon(TokenIt &it, const Tokens &tokens, const std::string &directive);

struct ListenConfig
{
    std::string host;
    int port;
};

struct Location
{
    std::string path;
    std::string root;
    int redirectCode;
    std::string cgiExt;
    std::string cgiPath;
    std::string autoindex;
    std::string redirectTarget;
    std::string uploadEnabled;
    std::string uploadPath;
    std::vector<std::string> indexes;
    std::vector<std::string> methods;
    Location() : redirectCode(0), autoindex("off"), uploadEnabled("off") {}
};

struct ServerConfig
{
    int port;
    std::string host;
    std::string root;
	ssize_t client_max_body_size;
    std::vector<Location> Locations;
    std::vector<std::string> indexes;
    std::vector<std::string> serverName;
    std::map<int, std::string> errorPage;
    ServerConfig() : port(80), host("127.0.0.1"), root("./www") {}
};



#include "configParser.hpp"

std::string expect(TokenIt &it, const Tokens &tokens, const std::string &err)
{
    if (it == tokens.end())
        throw std::runtime_error(err);
    return *it++;
}

void expectSemicolon(TokenIt &it, const Tokens &tokens, const std::string &directive)
{
    if (expect(it, tokens, "Missing ';' after " + directive) != ";")
        throw std::runtime_error("Expected ';' after " + directive);
}

int parsePort(TokenIt &it, const Tokens &tokens)
{
    std::string value = expect(it, tokens, "Missing port");
	if (value.size() > 5)
        throw std::runtime_error("Invalid port: " + value);
    int port;
    try
    {
        port = std::atoi(value.c_str());
    }
    catch (...)
    {
        throw std::runtime_error("Invalid port: " + value);
    }
    if (0 > port || port > 65535)
        throw std::runtime_error("Invalid port: " + value);
    return static_cast<int>(port);
}

size_t parseBodySize(TokenIt &it, const Tokens &tokens)
{
    std::string value = expect(it, tokens, "Missing body size value");
    try
    {
        return myStold(value);
    }
    catch (...)
    {
        throw std::runtime_error("Invalid client_max_body_size value: " + value);
    }
}

std::string parseRoot(TokenIt &it, const Tokens &tokens)
{
    std::string root = expect(it, tokens, "Missing root");

    root = mergeSlashes(root);

    while (root.size() > 1 && root[root.size() - 1] == '/')
        root.erase(root.size() - 1);

    if (root.empty())
        throw std::runtime_error("Invalid root path");

    if (root.find("..") != std::string::npos)
        throw std::runtime_error("Invalid root path: directory traversal");

    return root;
}

std::string parseLocationPath(TokenIt &it, const Tokens &tokens)
{
    std::string path = expect(it, tokens, "Missing location path");

    path = mergeSlashes(path);

    if (path.empty() || path[0] != '/')
        throw std::runtime_error("Location path must start with '/'");

    while (path.size() > 1 && path[path.size() - 1] == '/')
        path.erase(path.size() - 1);

    if (path.find("..") != std::string::npos)
        throw std::runtime_error("Invalid location path: directory traversal");

    return path;
}

std::vector<std::string> parseIndexes(TokenIt &it, const Tokens &tokens)
{
    std::vector<std::string> indexes;
    while (it != tokens.end() && *it != ";")
    {
        std::string index = expect(it, tokens, "Missing index value");

        index = mergeSlashes(index);

        while (!index.empty() && index[0] == '/')
            index.erase(0, 1);

        if (index.empty())
            throw std::runtime_error("Index cannot be empty");
		
        if (index.find("..") != std::string::npos)
            throw std::runtime_error("Invalid index path: directory traversal");

        indexes.push_back(index);
    }
    if (it == tokens.end())
        throw std::runtime_error("Missing ';' after index");

    if (indexes.empty())
        throw std::runtime_error("index directive requires at least one file");
    ++it;
    return indexes;
}

std::string parseCgiExt(TokenIt &it, const Tokens &tokens)
{
    std::string ext = expect(it, tokens, "Missing cgi_ext");

    if (ext.empty())
        throw std::runtime_error("Invalid cgi_ext");
    if (ext.find("..") != std::string::npos || ext.find("/") != std::string::npos)
        throw std::runtime_error("Invalid cgi_ext");
    if (ext[0] != '.')
        ext = "." + ext;
    return ext;
}

std::string parseErrorPagePath(const std::string& raw)
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

#include "configParser.hpp"

std::vector<std::string> prepConf(const std::string& filepath)
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

Location parseLocation(const std::vector<std::string> &tokens, std::vector<std::string>::iterator &it)
{
    Location loc;

    loc.path = parseLocationPath(it, tokens);

    if (expect(it, tokens, "Expected '{'") != "{")
        throw std::runtime_error("Expected '{'");

    while (it != tokens.end())
    {
        if (*it == "}")
        {
            if (loc.uploadEnabled == "on" && loc.uploadPath.empty())
                throw std::runtime_error("upload on requires upload_path");

            if (loc.uploadEnabled == "off" && !loc.uploadPath.empty())
                throw std::runtime_error("upload_path set but upload is off");

            if (!loc.cgiExt.empty() && loc.cgiPath.empty())
                throw std::runtime_error("cgi_ext requires cgi_path");

            if (!loc.cgiPath.empty() && loc.cgiExt.empty())
                throw std::runtime_error("cgi_path requires cgi_ext");

            ++it;
            return loc;
        }
        std::string key = *it;

        if (key == "methods")
        {
            ++it;

            while (it != tokens.end() && *it != ";")
                loc.methods.push_back(*it++);

            if (it == tokens.end())
                throw std::runtime_error("Missing ';' after methods");

            ++it;
        }
        else if (key == "root")
        {
            loc.root = parseRoot(++it, tokens);
            expectSemicolon(it, tokens, "root");
        }
        else if (key == "autoindex")
        {
            loc.autoindex = expect(++it, tokens, "Missing autoindex value");

            if (loc.autoindex != "on" && loc.autoindex != "off")
                throw std::runtime_error("autoindex must be 'on' or 'off'");

            expectSemicolon(it, tokens, "autoindex");
        }
        else if (key == "index")
        {
			++it;
			loc.indexes = parseIndexes(it, tokens);
        }
        else if (key == "cgi_path")
        {
            loc.cgiPath = parseRoot(++it, tokens);
            expectSemicolon(it, tokens, "cgi_path");
        }
        else if (key == "cgi_ext")
        {
            loc.cgiExt = parseCgiExt(++it, tokens);
            expectSemicolon(it, tokens, "cgi_ext");
        }
        else if (key == "redirect")
        {
            if (loc.redirectCode != 0)
                throw std::runtime_error("duplicate redirect directive");

            loc.redirectCode = std::atoi(expect(++it, tokens, "Missing redirect code").c_str());

            if (loc.redirectCode != 301 && loc.redirectCode != 302)
                throw std::runtime_error("Invalid redirect code");

            loc.redirectTarget = expect(it, tokens, "Missing redirect target");

            if (loc.redirectTarget.empty())
                throw std::runtime_error("Missing redirect target");
            if (loc.redirectTarget.find("..") != std::string::npos)
                throw std::runtime_error("Invalid redirect target");
            if (loc.redirectTarget[0] != '/' && loc.redirectTarget.find("http://") != 0 && loc.redirectTarget.find("https://") != 0)
                throw std::runtime_error("redirect target must be path or URL");
            expectSemicolon(it, tokens, "redirect");
        }
        else if (key == "upload")
        {
            if (!loc.uploadEnabled.empty())
                throw std::runtime_error("duplicate upload directive");

            loc.uploadEnabled = expect(++it, tokens, "Missing upload value");

            if (loc.uploadEnabled != "on" && loc.uploadEnabled != "off")
                throw std::runtime_error("upload must be 'on' or 'off'");

            expectSemicolon(it, tokens, "upload");
        }
        else if (key == "upload_path")
        {
            if (!loc.uploadPath.empty())
                throw std::runtime_error("duplicate upload_path directive");

            loc.uploadPath = parseRoot(++it, tokens);

            expectSemicolon(it, tokens, "upload_path");
        }
        else
        {
            throw std::runtime_error("Unknown location directive: " + key);
        }
    }
    throw std::runtime_error("Unclosed location block");
}
void parseErrorPage(ServerConfig &conf, std::vector<std::string>::iterator &it, const std::vector<std::string> &tokens)
{
    std::vector<std::string> values;

    for (++it; it != tokens.end() && *it != ";"; ++it)
        values.push_back(*it);

    if (it == tokens.end())
        throw std::runtime_error("Missing ';' after error_page");

    if (values.size() < 2)
        throw std::runtime_error("Invalid error_page syntax: missing codes or path.");

    std::string path = parseErrorPagePath(values.back());

    for (size_t i = 0; i < values.size() - 1; ++i)
    {
        unsigned long code;
        try
        {
            code = myStold(values[i]);
        }
        catch (...)
        {
            throw std::runtime_error("Invalid error code in config: " + values[i]);
        }
        if (code < 300 || code > 599)
            throw std::runtime_error("Invalid error code in config: " + values[i]);
        conf.errorPage[static_cast<int>(code)] = path;
    }
}

struct CompareLocations
{
    bool operator()(const Location &a, const Location &b) const
    {
        return a.path.size() > b.path.size();
    }
};

ServerConfig parseServer(const std::vector<std::string>& tokens, std::vector<std::string>::iterator &it)
{
    ServerConfig conf;

    if (expect(it, tokens, "Expected '{'") != "{")
        throw std::runtime_error("Expected '{'");
    while (it != tokens.end())
    {
        if (*it == "}")
        {
            ++it;
            return conf;
        }
        std::string key = *it;
        if (key == "host")
        {
            conf.host = expect(++it, tokens, "Missing host");
            expectSemicolon(it, tokens, "host");
        }
        else if (key == "listen")
        {
            conf.port = parsePort(++it, tokens);
            expectSemicolon(it, tokens, "listen");
        }
        else if (key == "root")
        {
            conf.root = parseRoot(++it, tokens);
            expectSemicolon(it, tokens, "root");
        }
        else if (key == "server_name")
        {
            ++it;
            while (it != tokens.end() && *it != ";")
                conf.serverName.push_back(*it++);
            if (it == tokens.end())
                throw std::runtime_error("Missing ';' after server_name");
            ++it;
        }
        else if (key == "location")
        {
            ++it;
            conf.Locations.push_back(parseLocation(tokens, it));
        }
        else if (key == "index")
        {
           	++it;
			conf.indexes = parseIndexes(it, tokens);
        }
        else if (key == "error_page")
        {
            parseErrorPage(conf, it, tokens);
			if (it == tokens.end())
    			throw std::runtime_error("Missing ';' after error_page");
            ++it;
        }
        else if (key == "client_max_body_size")
        {
            conf.client_max_body_size = parseBodySize(++it, tokens);
            expectSemicolon(it, tokens, "client_max_body_size");
        }
        else
            throw std::runtime_error("Unknown server directive: " + key);
    }
    throw std::runtime_error("Unclosed server block");
}

std::vector<ServerConfig> parseTokens(std::vector<std::string> tokens)
{
    std::vector<ServerConfig> allServers;
    std::vector<std::string>::iterator it = tokens.begin();

    while (it != tokens.end())
    {
        if (*it != "server")
            throw std::runtime_error("Expected 'server' keyword");
        ++it;
        ServerConfig currServerConfig = parseServer(tokens, it);
        std::sort(
            currServerConfig.Locations.begin(),
            currServerConfig.Locations.end(),
            CompareLocations()
        );
        allServers.push_back(currServerConfig);
    }
    return allServers;
}


std::vector<ServerConfig> parseConfig(std::string configFile)
{
    std::vector<std::string> tokens = prepConf(configFile);
	std::vector<ServerConfig> allServers = parseTokens(tokens); 
    return allServers;
}


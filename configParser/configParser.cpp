#include "configParser.hpp"
#include <stdexcept>
#include <iostream>
#include <stdlib.h>
#include <fstream>
#include <sstream>
#include "../Helpers.hpp"
#include <algorithm>

// std::vector<std::string> prepConf(const std::string& filepath)
// {
//     std::vector<std::string> tokens;
//     std::ifstream myconf(filepath.c_str());
//     std::string specials = "{;}";
//     std::string cleanConf;

//     if ( myconf.is_open() ) {
//         char mychar;
//         while ((mychar = myconf.get()) != -1) {
//             if (mychar == '#')
//             {
//                 while ((mychar = myconf.get()) != -1)
//                 {
//                     if (mychar == '\n')
//                         break;
//                 }
//             }
//             else if (specials.find(mychar) < specials.length())
//             {
//                 cleanConf += ' ';
//                 cleanConf += mychar;
//                 cleanConf += ' ';
//             }
//             else
//                 cleanConf += mychar;
//         }
//         std::stringstream ss(cleanConf);
//         std::string buff;
//         while (ss >> buff)
//             tokens.push_back(buff);
//     }
//     else
//         std::cout << "Could not open config file" << std::endl; // or throw std::runtime_error("Could not open file");
//     return tokens;
// }

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

// Helper to get next token safely 
std::string expect(std::vector<std::string>::iterator &it, const std::vector<std::string> &tokens, const std::string &err) 
{
    if (it == tokens.end())
        throw std::runtime_error(err);
    return *it++;
}

Location parseLocation(const std::vector<std::string>& tokens, std::vector<std::string>::iterator &it)
{
    Location loc;
    loc.path = expect(it, tokens, "Missing path for location");
    
    if (expect(it, tokens, "Expected '{'") != "{") 
        throw std::runtime_error("Expected '{'");

    for (; it != tokens.end(); ++it)
    {
        if (*it == "}") 
            return loc;
        
        std::string key = *it;
        if (key == "methods") 
        {
            for (++it; it != tokens.end() && *it != ";"; ++it) 
                loc.methods.push_back(*it);
        } 
        else if (key == "root")      loc.root = expect(++it, tokens, "Missing root path");
        else if (key == "autoindex") loc.autoindex = expect(++it, tokens, "Missing autoindex value");
        else if (key == "index")     loc.index = expect(++it, tokens, "Missing index value");
        else if (key == "cgi_path")  loc.cgiPath = expect(++it, tokens, "Missing cgi_path");
        else if (key == "cgi_ext")   loc.cgiExt = expect(++it, tokens, "Missing cgi_ext");
        
        if (it == tokens.end()) break;
    }
    throw std::runtime_error("Unclosed location block");
}

void parseErrorPage(ServerConfig &conf, std::vector<std::string>::iterator &it, const std::vector<std::string> &tokens)
{
    std::vector<std::string> codes;
    for (++it; it != tokens.end() && *it != ";"; ++it)
        codes.push_back(*it);
        
    if (codes.size() < 2)
        throw std::runtime_error("Invalid error_page syntax: missing codes or path.");
        
    std::string path = codes.back();
    for (size_t i = 0; i < codes.size() - 1; ++i)
    {
        try 
        {
            int code = std::atoi(codes[i].c_str());
            conf.errorPage[code] = path;
        } catch (...) 
        {
            throw std::runtime_error("Invalid error code in config: " + codes[i]);
        }
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

    for (; it != tokens.end(); ++it)
    {
        if (*it == "}") 
            return conf;
            
        std::string key = *it;
        if (key == "host")            conf.host = expect(++it, tokens, "Missing host");
        else if (key == "listen")     conf.port = std::atoi(expect(++it, tokens, "Missing port").c_str());
        else if (key == "root")       conf.root = expect(++it, tokens, "Missing root");
        else if (key == "server_name") 
        {
            for (++it; it != tokens.end() && *it != ";"; ++it) 
                conf.serverName.push_back(*it);
        }
        else if (key == "location")   conf.Locations.push_back(parseLocation(tokens, ++it));
        else if (key == "index")   	  conf.index = expect(++it, tokens, "Missing host");
        else if (key == "error_page") parseErrorPage(conf, it, tokens);
		else if (key == "client_max_body_size") 
		{
			std::string val = expect(++it, tokens, "Missing body size value");
			try 
			{
				conf.client_max_body_size = myStoul(val);
				if (conf.client_max_body_size < 0)
					throw std::runtime_error("Invalid client_max_body_size value: " + val);
			} catch (...) 
			{
				throw std::runtime_error("Invalid client_max_body_size value: " + val);
			}
		}
    }
    throw std::runtime_error("Unclosed server block");
}

std::vector<ServerConfig> parseTokens(std::vector<std::string> tokens)
{
    std::vector<ServerConfig> allServers;
    std::vector<std::string>::iterator it = tokens.begin(); 
    ServerConfig currServerConfig;
    while (it != tokens.end()) 
    {
		std::cout << *it << std::endl;
        if (*it != "server") 
            throw std::runtime_error("Expected 'server' keyword"); 
        currServerConfig = parseServer(tokens, ++it);
		std::sort(currServerConfig.Locations.begin(), currServerConfig.Locations.end(), CompareLocations());
		allServers.push_back(currServerConfig);
		it++;
    }
    return allServers;
}


std::vector<ServerConfig> GETconfig()
{
    std::vector<std::string> tokens = prepConf("server.conf");
	std::vector<ServerConfig> allServers = parseTokens(tokens); 
    return allServers;
}


// sortedLocations = Config->Locations;
// 

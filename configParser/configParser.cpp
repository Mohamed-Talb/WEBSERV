#include "configParser.hpp"
#include <stdexcept>
#include <iostream>
#include <stdlib.h>
#include <fstream>
#include <sstream>

std::vector<std::string> prepConf(const std::string& filepath)
{
    std::vector<std::string> tokens;
    std::ifstream myconf(filepath.c_str());
    std::string specials = "{;}";
    std::string cleanConf;

    if ( myconf.is_open() ) {
        char mychar;
        while ((mychar = myconf.get()) != -1) {
            if (mychar == '#')
            {
                while ((mychar = myconf.get()) != -1)
                {
                    if (mychar == '\n')
                        break;
                }
            }
            else if (specials.find(mychar) < specials.length())
            {
                cleanConf += ' ';
                cleanConf += mychar;
                cleanConf += ' ';
            }
            else
                cleanConf += mychar;
        }
        std::stringstream ss(cleanConf);
        std::string buff;
        while (ss >> buff)
            tokens.push_back(buff);
    }
    else
        std::cout << "Could not open config file" << std::endl; // or throw std::runtime_error("Could not open file");
    return tokens;
}

Location parseLocation(std::vector<std::string> tokens, std::vector<std::string>::iterator &it)
{
    Location location;
    
    location.path = *(it++);
    if (*it != "{")
        throw std::runtime_error("No opening curly braces");
    
    for (it++; it != tokens.end(); it++)
    {
        if (*it == "}")
            return location;
        else if (*it == "methods")
        {
            it++;
            while (*it != ";")
                location.methods.push_back(*(it++));
        }
        else if (*it == "root")
            location.root = *(++it);
        else if (*it == "autoindex")
            location.autoindex = *(++it);
        else if (*it == "index")
            location.index = *(++it);
        else if (*it == "cgi_path")
            location.cgiPath = *(++it);
        else if (*it == "cgi_ext")
            location.cgiExt = *(++it);
    }

    throw std::runtime_error("No matching curly braces");
}

ServerConfig parseServer(std::vector<std::string> tokens, std::vector<std::string>::iterator &it)
{
    ServerConfig serverConf;
    
    if (*it != "{")
        throw std::runtime_error("No opening curly braces");
    
    for (it++; it != tokens.end(); it++)
    {
        if (*it == "}")
            return serverConf;
        else if (*it == "host")
            serverConf.host = *(++it); // this is not readable
        else if (*it == "listen")
            serverConf.port = std::atoi((*(++it)).c_str());
        else if (*it == "server_name")
        {
            it++;
            while (*it != ";")
                serverConf.serverName.push_back(*(it++));
        }
        else if (*it == "root")
            serverConf.root = *(++it);
        else if (*it == "location")
        {
            Location location = parseLocation(tokens, ++it);
            serverConf.Locations.push_back(location);
        }
        else if (*it == "error_page")
        {
            std::vector<int> errorCodes;

            while (true)
            {
                ++it;
                if (it + 1 == tokens.end())
                    throw std::runtime_error("ending semicolon not found.");
                else if(*(it + 1) == ";")
                    break;
                errorCodes.push_back(std::atoi((*it).c_str()));
            }
            for (std::vector<int>::iterator tempIt = errorCodes.begin(); tempIt != errorCodes.end(); tempIt++)
                serverConf.errorPage.insert(std::pair<int, std::string>(*tempIt, *it));
            it++; // skip error page
        }
    }

    throw std::runtime_error("No matching curly braces");
}

std::vector<ServerConfig> parseTokens(std::vector<std::string> tokens)
{
    std::vector<ServerConfig> allServers;
    std::vector<std::string>::iterator it;
    ServerConfig serverConf;

    for (it = tokens.begin(); it != tokens.end(); it++)
    {
        if (*it != "server")
            throw std::runtime_error("No server config found."); // change later to custom syntax/parsing exception
        serverConf = parseServer(tokens, ++it);
        allServers.push_back(serverConf);
    }
    return allServers;
}


std::vector<ServerConfig> GETconfig()
{
    std::vector<std::string> tokens = prepConf("server.conf");
    std::vector<ServerConfig> allServers = parseTokens(tokens); 
    return allServers;
}


// int main()
// {
//     std::vector<std::string> tokens = prepConf("server.conf");
//     std::vector<ServerConfig> allServers = parseTokens(tokens);    
    
//     // ServerConfig myServer = allServers[0];
//     // std::cout << myServer.host << std::endl;
//     // std::cout << myServer.port << std::endl;
//     // std::cout << "server names: ";
//     // for (std::vector<std::string>::iterator tempIt = myServer.serverName.begin(); tempIt != myServer.serverName.end(); tempIt++)
//     //     std::cout << *tempIt << " ";
//     // std::cout << std::endl;
//     // std::cout << myServer.root << std::endl;
//     // std::cout << "Locations:" << std::endl;
//     // for (std::vector<Location>::iterator tempIt = myServer.Locations.begin(); tempIt != myServer.Locations.end(); tempIt++)
//     // {
//     //     std::cout << "methodes: ";
//     //     for (std::vector<std::string>::iterator tempIt2 = (*tempIt).methods.begin(); tempIt2 != (*tempIt).methods.end(); tempIt2++)
//     //     {
//     //         std::cout << *tempIt2 << " ";
//     //     }
//     //     std::cout << std::endl;
//     //     std::cout << (*tempIt).path << std::endl;
//     //     std::cout << (*tempIt).root << std::endl;
//     //     std::cout << (*tempIt).autoindex << std::endl;
//     //     std::cout << (*tempIt).index << std::endl;
//     //     std::cout << (*tempIt).cgiPath << std::endl;
//     //     std::cout << (*tempIt).cgiExt << std::endl;
//     //     std::cout << "--------------------" << std::endl;
//     // }
//     // std::cout << "error pages:" << std::endl;
//     // for (std::map<int, std::string>::iterator tempIt = myServer.errorPage.begin(); tempIt != myServer.errorPage.end(); tempIt++)
//     // {
//     //     std::cout << (*tempIt).first << std::endl;
//     //     std::cout << (*tempIt).second << std::endl;
//     // }
// }

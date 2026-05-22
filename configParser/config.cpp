#include "config.hpp"

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

ServerConfig::ServerConfig() :
    root("./www"), 
    client_max_body_size(1048576)
{
    indexes.push_back("index.html");
    serverName.push_back(""); 
}

void ServerConfig::finalizeAndValidate() 
{
    if (listens.empty()) 
        listens.push_back(Listen());
    for (size_t i = 0; i < Locations.size(); ++i)
    {
        Location &loc = Locations[i];
        if (loc.root.empty()) 
        {
            loc.root = this->root;
        }
        if (loc.indexes.empty()) 
        {
            loc.indexes = this->indexes;
        }
        loc.validateLocation();
    }
}
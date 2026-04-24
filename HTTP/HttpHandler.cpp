#include "HttpHandler.hpp"
#include "../CGI/CGI.hpp"
#include "Methods.hpp" 
#include "HttpUtils.hpp"

struct CompareLocations
{
    bool operator()(const Location& a, const Location& b) const
    {
        return a.path.size() > b.path.size();
    }
};

HttpHandler::HttpHandler(const ServerConfig &serverConfig) : Config(&serverConfig)
{
    sortedLocations = Config->Locations;
    std::sort(sortedLocations.begin(), sortedLocations.end(), CompareLocations());
}

HttpHandler::~HttpHandler() {}

const Location* HttpHandler::matchLocation(const std::string& path)
{
    for (size_t i = 0; i < Config->Locations.size(); ++i)
    {
        const Location &loc = sortedLocations[i];
        if (path.compare(0, loc.path.size(), loc.path) == 0)
        {
            return &loc;
        }
    }
    return NULL;
}

bool HttpHandler::isMethodAllowed(const std::string& method, const Location& loc)
{
    if (loc.methods.empty())
        return true; 
        
    for (size_t i = 0; i < loc.methods.size(); ++i)
    {
        if (toUpper(loc.methods[i]) == method)
            return true;
    }
    return false;
}

const Location *HttpHandler::getCgiLocation(const HttpRequest& request)
{
    std::string requestPath = HttpUtils::stripQuery(request.getTarget());
    const Location *matchedLocation = matchLocation(requestPath);
    
    if (!matchedLocation) return NULL;

    if (!matchedLocation->cgiExt.empty())
    {
        if (requestPath.size() >= matchedLocation->cgiExt.size() &&
            requestPath.compare(requestPath.size() - matchedLocation->cgiExt.size(), 
                                matchedLocation->cgiExt.size(), matchedLocation->cgiExt) == 0)
        {
            return matchedLocation;
        }
    }
    return NULL;
}

HttpResponse HttpHandler::process(const HttpRequest& request)
{
    if (request.getErrorCode() != 0)
    {
        return HttpUtils::ErrorPage(request.getErrorCode(), "Bad Request", *Config);
    }

    std::string method = request.getMethod();
    std::string requestPath = HttpUtils::stripQuery(request.getTarget());
    
    const Location* matchedLocation = matchLocation(requestPath);
    if (!matchedLocation)
        return HttpUtils::ErrorPage(404, "Not Found", *Config);

    if (!isMethodAllowed(method, *matchedLocation))
        return HttpUtils::ErrorPage(405, "Method Not Allowed", *Config);
    if (method == "GET") 
        return HttpMethods::GET(Config->root, requestPath, *Config);
    // if (method == "DELETE")
    //     return HttpMethods::DELETE(Config->root, requestPath);
    return HttpUtils::ErrorPage(501, "Not Implemented", *Config);
}


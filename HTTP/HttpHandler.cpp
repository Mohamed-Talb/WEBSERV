#include "HttpHandler.hpp"
#include "../CGI/CGI.hpp"
#include "Methods.hpp" 
#include "HttpUtils.hpp"
#include <sys/stat.h>

HttpHandler::HttpHandler(const ServerConfig &serverConfig) : serverConfig(&serverConfig) {}

HttpHandler::~HttpHandler() {}

const Location *HttpHandler::matchLocation(const std::string& path)
{
    for (size_t i = 0; i < serverConfig->Locations.size(); ++i)
    {
        const Location& loc = serverConfig->Locations[i];
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

std::string HttpHandler::resolveIndexFile(const Location *loc)
{
    if (loc && !loc->index.empty())
        return loc->index;
    if (!serverConfig->index.empty())
        return serverConfig->index;
    return "index.html";
}

HttpResponse HttpHandler::process(const HttpRequest &request)
{
    if (request.getErrorCode() != 0)
    {
        return HttpUtils::ErrorPage(request.getErrorCode(), "Bad Request", *serverConfig);
    }
    std::string method = request.getMethod();
    std::string requestPath = HttpUtils::stripQuery(request.getTarget());
    const Location *matchedLocation = matchLocation(requestPath);
    if (!matchedLocation)
        return HttpUtils::ErrorPage(404, "Not Found", *serverConfig);

    if (!isMethodAllowed(method, *matchedLocation))
    {
        return HttpUtils::ErrorPage(405, "Method Not Allowed", *serverConfig);
    }
	struct stat S;
    if (stat(requestPath.c_str(), &S) == 0)
		if (S_ISDIR(S.st_mode))
			requestPath = resolveIndexFile(matchedLocation);
    if (method == "GET") 
        return HttpMethods::GET(serverConfig->root, requestPath, *serverConfig);
    if (method == "DELETE")
        return HttpMethods::DELETE(serverConfig->root, requestPath, *serverConfig);
    return HttpUtils::ErrorPage(501, "Not Implemented", *serverConfig);
}


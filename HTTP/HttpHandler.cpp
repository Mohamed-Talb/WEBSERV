#include "HttpHandler.hpp"
#include "../CGI/CGI.hpp"
#include "Methods.hpp" 
#include "HttpUtils.hpp"
#include <sys/stat.h>
#include <dirent.h>

HttpHandler::HttpHandler(const ServerConfig &serverConfig) : serverConfig(&serverConfig) {}

HttpHandler::~HttpHandler() {}

const Location* HttpHandler::matchLocation(const std::string &path)
{
    const Location* bestMatch = NULL;
    size_t bestLength = 0;

    for (size_t i = 0; i < serverConfig->Locations.size(); ++i)
    {
        const Location& loc = serverConfig->Locations[i];
        if (path.compare(0, loc.path.size(), loc.path) == 0)
        {
            if (loc.path == "/" || path.size() == loc.path.size() || path[loc.path.size()] == '/')
            {
                if (loc.path.size() > bestLength)
                {
                    bestMatch = &loc;
                    bestLength = loc.path.size();
                }
            }
        }
    }
    return bestMatch;
}

bool HttpHandler::isMethodAllowed(const std::string &method, const Location &loc)
{
    std::cout << loc.path << std::endl;   
    std::cout << loc.root << std::endl;   
    if (loc.methods.empty())
        return true;
    for (size_t i = 0; i < loc.methods.size(); ++i)
    {
        std::cout << loc.methods[i] << std::endl;
        if (toUpper(loc.methods[i]) == method)
            return true;
    }
    return false;
}

const Location *HttpHandler::getCgiLocation(const HttpRequest &request)
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

std::vector<std::string> HttpHandler::resolveIndexFiles(const Location *loc)
{
    if (loc && !loc->indexes.empty())
        return loc->indexes;
    if (!serverConfig->indexes.empty())
        return serverConfig->indexes;
    std::vector<std::string> defaults;
    defaults.push_back("index.html");
    return defaults;
}

void HttpHandler::resolveRoute(const HttpRequest& request, RouteMatch& match)
{
    match.location = NULL;
    match.requestPath.clear();
    match.root.clear();
    match.fullPath.clear();

    std::string requestPath = HttpUtils::stripQuery(request.getTarget());

    const Location* location = matchLocation(requestPath);
    if (!location)
        return;

    std::string root = location->root.empty() ? serverConfig->root : location->root;
    std::string relativePath = requestPath;
    if (location->path != "/")
    {
        relativePath = requestPath.substr(location->path.size());
        if (relativePath.empty())
            relativePath = "/";
    }
    std::string fullPath = joinPath(root, relativePath);
    match.location = location;
    match.requestPath = requestPath;
    match.root = root;
    match.fullPath = fullPath;
}

HttpResponse generateDirectoryListing(std::string fullPath, std::string requestPath)
{
    
}

HttpResponse HttpHandler::process(const HttpRequest& request)
{
    if (request.getErrorCode() != 0)
        return HttpUtils::ErrorPage(request.getErrorCode(), "Bad Request", *serverConfig);

    RouteMatch match;
    resolveRoute(request, match);
    if (match.location->redirectCode != 0)
    {
        int code = match.location->redirectCode;
        std::string reason = code == 301 ? "Moved Permanently" : "Found";
        HttpResponse res(code, reason);
        res.setHeader("Location", match.location->redirectTarget);
        res.setHeader("Content-Length", "0");
        return res;
    }
    std::string method = request.getMethod();

    if (!match.location)
        return HttpUtils::ErrorPage(404, "Not Found", *serverConfig);

    if (!isMethodAllowed(method, *match.location))
        return HttpUtils::ErrorPage(405, "Method Not Allowed", *serverConfig);

    struct stat S;
    if (stat(match.fullPath.c_str(), &S) == 0 && S_ISDIR(S.st_mode))
    {
        std::vector<std::string> indexes = resolveIndexFiles(match.location);
        bool foundIndex = false;
        for (size_t i = 0; i < indexes.size(); ++i)
        {
            std::string candidatePath = joinPath(match.fullPath, indexes[i]);

            if (FileSystem::fileExists(candidatePath))
            {
                match.requestPath = joinPath(match.requestPath, indexes[i]);
                match.fullPath = candidatePath;
                foundIndex = true;
                break;
            }
        }

        if (!foundIndex)
            return HttpUtils::ErrorPage(403, "Forbidden", *serverConfig);
    }

    if (method == "GET")
        return HttpMethods::GET(&match, *serverConfig);

    if (method == "DELETE")
        return HttpMethods::DELETE(&match, *serverConfig);

    if (method == "POST")
        return HttpMethods::POST(request, &match, *serverConfig);

    return HttpUtils::ErrorPage(501, "Not Implemented", *serverConfig);
}
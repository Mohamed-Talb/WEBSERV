#include "HttpHandler.hpp"


HttpHandler::HttpHandler(const ServerConfig &serverConfig) : serverConfig(&serverConfig) {}

HttpHandler::~HttpHandler() {}

const Location* HttpHandler::matchLocation(const std::string &path)
{
    const Location* bestMatch = NULL;
    size_t bestLength = 0;
    for (size_t i = 0; i < serverConfig->locations.size(); ++i)
    {
        const Location &loc = serverConfig->locations[i];
        if (path.size() >= loc.path.size() && path.compare(0, loc.path.size(), loc.path) == 0)
        {
            if (loc.path == "/" || 
                path.size() == loc.path.size() || 
                loc.path[loc.path.size() - 1] == '/' || 
                path[loc.path.size()] == '/')
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
    if (loc.methods.empty())
        return true;
    for (size_t i = 0; i < loc.methods.size(); ++i)
    {
        if (toUpper(loc.methods[i]) == method)
            return true;
    }
    return false;
}

const Location *HttpHandler::getCgiLocation(const HttpRequest &request)
{
    std::string requestPath = request.getRequestPath();
    const Location *matchedLocation = matchLocation(requestPath);
    
    if (!matchedLocation || matchedLocation->cgiExt.empty()) 
        return NULL;
    if (requestPath.size() >= matchedLocation->cgiExt.size())
    {
        size_t extOffset = requestPath.size() - matchedLocation->cgiExt.size();
        if (requestPath.compare(extOffset, matchedLocation->cgiExt.size(), matchedLocation->cgiExt) == 0)
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

void HttpHandler::resolveRoute(const HttpRequest &request, RouteMatch& match)
{
    match.location = NULL;
    match.requestPath = request.getRequestPath();
    
    const Location *location = matchLocation(match.requestPath);
    if (location)
    {
        match.location = location;
        match.root = location->root; 
        
        std::string relativePath = match.requestPath;
        if (location->path != "/")
        {
            if (match.requestPath.size() >= location->path.size())
            {
                relativePath = match.requestPath.substr(location->path.size());
            }
            if (relativePath.empty() || relativePath[0] != '/')
            {
                relativePath = "/" + relativePath;
            }
        }
        match.fullPath = joinPath(match.root, relativePath);
    }
    else
    {
        match.root = serverConfig->root;
        match.fullPath = joinPath(match.root, match.requestPath);
    }
}

HttpResponse resolveRedirection(const RouteMatch &match)
{
    int code = match.location->redirectCode;

    std::string reason;
    switch (code)
    {
        case 301: reason = "Moved Permanently"; break;
        case 302: reason = "Found"; break;
        case 303: reason = "See Other"; break;
        case 307: reason = "Temporary Redirect"; break;
        case 308: reason = "Permanent Redirect"; break;
        default:
            HttpResponse error(500, "Internal Server Error");
            error.setHeader("Content-Length", "0");
            return error;
    }
    if (match.location->redirectTarget.empty())
    {
        HttpResponse error(500, "Internal Server Error");
        error.setHeader("Content-Length", "0");
        return error;
    }

    HttpResponse response(code, reason);
    response.setHeader("Location", match.location->redirectTarget);
    response.setHeader("Content-Length", "0");

    return response;
}


HttpResult HttpHandler::process(const HttpRequest& request)
{
    if (request.getErrorCode() != 0)
    {
        return HttpResult::makeResponse(ErrorPage(request.getErrorCode(), *serverConfig));
    }

    RouteMatch match;
    resolveRoute(request, match);
    if (match.location && match.location->redirectCode != 0)
    {
        return HttpResult::makeResponse(resolveRedirection(match));
    }
    std::string method = request.getMethod();
    bool allowed = match.location ? isMethodAllowed(method, *match.location) : (method == "GET");
    
    if (!allowed)
    {
        return HttpResult::makeResponse(ErrorPage(405, *serverConfig));
    }

    if (request.getBody().size() > static_cast<size_t>(serverConfig->client_max_body_size))
    {
        return HttpResult::makeResponse(ErrorPage(413, *serverConfig));
    }

    if (isDirectory(match.fullPath))
    {
        std::vector<std::string> indexes = resolveIndexFiles(match.location);
        bool foundIndex = false;
        for (size_t i = 0; i < indexes.size(); ++i)
        {
            std::string candidatePath = joinPath(match.fullPath, indexes[i]);
            if (fileExists(candidatePath))
            {
                match.requestPath = joinPath(match.requestPath, indexes[i]);
                match.fullPath = candidatePath;
                foundIndex = true;
                break;
            }
        }

        if (!foundIndex)
        {
            bool autoIndexOn = (match.location && match.location->autoindex == "on");
            if (method == "GET" && autoIndexOn)
            {
                return HttpResult::makeResponse(resolveAutoIndexing(match, *serverConfig));
            }
            return HttpResult::makeResponse(ErrorPage(403,*serverConfig));
        }
    }

    if (!fileExists(match.fullPath) && method != "POST")
    {
        return HttpResult::makeResponse(ErrorPage(404, *serverConfig));
    }
    if (match.location && !match.location->cgiExt.empty())
    {
        if (match.fullPath.size() >= match.location->cgiExt.size())
        {
            size_t extOffset = match.fullPath.size() - match.location->cgiExt.size();
            if (match.fullPath.compare(extOffset, match.location->cgiExt.size(), match.location->cgiExt) == 0)
            {
                return HttpResult::makeCgi(match.location, match.fullPath);
            }
        }
    }

    HttpResponse response;
    if (method == "GET")
    {
        response = HttpMethods::GET(match, *serverConfig);
    }
    else if (method == "DELETE")
    {
        response = HttpMethods::DELETE(match, *serverConfig);
    }
    else if (method == "POST")
    {
        response = HttpMethods::POST(request, match, *serverConfig);
    }
    else 
    {
        response = ErrorPage(501,*serverConfig);
    }

    return HttpResult::makeResponse(response);
}
#include "HttpHandler.hpp"


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
    std::string requestPath = request.getRequestPath();
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

void HttpHandler::resolveRoute(const HttpRequest &request, RouteMatch& match)
{
    match.location = NULL;
    match.requestPath.clear();
    match.root.clear();
    match.fullPath.clear();

    std::string requestPath = request.getRequestPath();

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
        return HttpResult::makeResponse(ErrorPage(request.getErrorCode(), "Bad Request", *serverConfig));

    RouteMatch match;
    resolveRoute(request, match);
    if (!match.location)
        return HttpResult::makeResponse(ErrorPage(404, "Not Found", *serverConfig));

    if (match.location->redirectCode != 0)
    {
        return HttpResult::makeResponse(resolveRedirection(match));
    }
    std::string method = request.getMethod();
    if (!isMethodAllowed(method, *match.location))
        return HttpResult::makeResponse(ErrorPage(405, "Method Not Allowed", *serverConfig));
    if (request.getBody().size() > static_cast<size_t>(serverConfig->client_max_body_size))
    {
        return HttpResult::makeResponse(ErrorPage(413, "Payload Too Large", *serverConfig));
    }
    const Location *cgiLocation = getCgiLocation(request);
    if (cgiLocation != NULL)
    {
        std::string requestPath = request.getRequestPath();
        return HttpResult::makeCgi(cgiLocation, requestPath);
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
            if (method == "GET" && match.location->autoindex == "on")
                return HttpResult::makeResponse(resolveAutoIndexing(match, *serverConfig));
            return HttpResult::makeResponse(ErrorPage(403, "Forbidden", *serverConfig));
        }
    }
    HttpResponse response;
    if (method == "GET")
        response = HttpMethods::GET(match, *serverConfig);
    else if (method == "DELETE")
       response = HttpMethods::DELETE(match, *serverConfig);
    else if (method == "POST")
        response = HttpMethods::POST(request, match, *serverConfig);
    else 
        response = ErrorPage(501, "Not Implemented", *serverConfig);
    return HttpResult::makeResponse(response);
}


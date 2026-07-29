#include "HttpHandler.hpp"

#include "../Helpers.hpp"
#include "./Methods/Methods.hpp"
#include "HttpUtils/HttpUtils.hpp"

HttpHandler::HttpHandler(const ServerConfig &config): serverConfig(&config) {}


bool HttpHandler::isMethodAllowed(const Location *location, const std::string &method) const
{
    if (!location)
        return method == "GET";

    return std::find(location->methods.begin(), location->methods.end(), method) != location->methods.end();
}
bool HttpHandler::isCgiRequest(const RouteMatch &match) const
{
    if (!match.location || match.location->cgiExt.empty())
        return false;

    const std::string &extension = match.location->cgiExt;

    if (match.fullPath.size() < extension.size())
        return false;

    size_t offset = match.fullPath.size() - extension.size();
    return match.fullPath.compare(offset, extension.size(), extension) == 0;
}

void HttpHandler::resolveRoute(const HttpRequest &request, RouteMatch &match) const
{
    match.requestPath = request.getRequestPath();
    match.location = matchLocation(*serverConfig, match.requestPath);

    if (match.location && !match.location->root.empty())
    {
        match.root = match.location->root;

        std::string relativePath = match.requestPath.substr(match.location->path.size());

        if (relativePath.empty())
            relativePath = "/";

        match.fullPath = joinPath(match.root, relativePath);
        return;
    }

    match.root = serverConfig->root;
    match.fullPath = joinPath(match.root, match.requestPath);
}

std::vector<std::string> HttpHandler::resolveIndexFiles(const Location *location) const
{
    if (location && !location->indexes.empty())
        return location->indexes;

    if (!serverConfig->indexes.empty())
        return serverConfig->indexes;

    std::vector<std::string> defaults;
    defaults.push_back("index.html");
    return defaults;
}

HttpResponse HttpHandler::resolveRedirection(const Location &location) const
{
    std::string reason;
    switch (location.redirectCode)
    {
        case 301:
            reason = "Moved Permanently";
            break;
        case 302:
            reason = "Found";
            break;
        case 303:
            reason = "See Other";
            break;
        case 307:
            reason = "Temporary Redirect";
            break;
        case 308:
            reason = "Permanent Redirect";
            break;
        default:
            return ErrorPage(500, *serverConfig);
    }

    if (location.redirectTarget.empty())
        return ErrorPage(500, *serverConfig);

    HttpResponse response(location.redirectCode, reason);
    response.setHeader("Location", location.redirectTarget);
    response.setHeader("Content-Length", "0");
    return response;
}

bool HttpHandler::resolveDirectory(RouteMatch &match, const HttpRequest &request, HttpResponse &response) const
{

    if (!isDirectory(match.fullPath))
        return true;
    
    if (match.requestPath.empty() || match.requestPath[match.requestPath.size() - 1] != '/')
    {
        std::string queryString = request.getQuery();
        std::string redirectTarget = match.requestPath + "/";

        if (!queryString.empty())
            redirectTarget += "?" + queryString;

        response = HttpResponse(301, "Moved Permanently");
        response.setHeader("Location", redirectTarget);
        response.setHeader("Content-Length", "0");
        return false;
    }

    std::vector<std::string> indexes = resolveIndexFiles(match.location);
    for (size_t i = 0; i < indexes.size(); ++i)
    {
        std::string candidate = joinPath(match.fullPath, indexes[i]);
        if (!isRegularFile(candidate))
            continue;
        match.requestPath = joinPath(match.requestPath, indexes[i]);
        match.fullPath = candidate;
        return true;
    }
    bool autoIndexEnabled = match.location && match.location->autoindex == "on";
    if (request.getMethod() == "GET" && autoIndexEnabled)
    {
        response = resolveAutoIndexing(match, *serverConfig);
        return false;
    }
    response = ErrorPage(404, *serverConfig);
    return false;
}

HttpResult HttpHandler::process(const HttpRequest &request) const
{
    std::cout << "[REQUEST]: " << request.getVersion() << " " << request.getMethod() << " " << request.getRequestPath() << std::endl;
    RouteMatch match;
    resolveRoute(request, match);
    std::cout << "[MAP TO]: " << match.fullPath << std::endl;
    if (match.location && match.location->redirectCode != 0)
    {
        return HttpResult::makeResponse(resolveRedirection(*match.location));
    }
    const std::string &method = request.getMethod();
    if (!isMethodAllowed(match.location, method))
    {
        return HttpResult::makeResponse(ErrorPage(405, *serverConfig));
    }
    HttpResponse directoryResponse;
    if (!resolveDirectory(match, request, directoryResponse))
        return HttpResult::makeResponse(directoryResponse);

    if (isCgiRequest(match))
        return HttpResult::makeCgi(match.location,match.fullPath);

    if (!fileExists(match.fullPath) && method != "POST")
    {
        return HttpResult::makeResponse(ErrorPage(404, *serverConfig));
    }
    if (method == "GET")
    {
        return HttpResult::makeResponse(HttpMethods::GET(match, *serverConfig));
    }
    if (method == "DELETE")
    {
        return HttpResult::makeResponse(HttpMethods::DELETE(match, *serverConfig));
    }
    if (method == "POST")
    {
        return HttpResult::makeResponse(HttpMethods::POST(request, match, *serverConfig));
    }
    return HttpResult::makeResponse(ErrorPage(501, *serverConfig));
}
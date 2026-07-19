#include "HttpHandler.hpp"

#include "../Helpers.hpp"
#include "./Methods/Methods.hpp"
#include "HttpUtils/HttpUtils.hpp"

HttpHandler::HttpHandler(const ServerConfig &config): serverConfig(&config) {}

const Location *HttpHandler::matchLocation(const std::string &path) const
{
    const Location *bestMatch = NULL;
    size_t bestLength = 0;

    for (size_t i = 0; i < serverConfig->locations.size(); ++i)
    {
        const Location &location = serverConfig->locations[i];

        if (path.size() < location.path.size())
            continue;

        if (path.compare(0, location.path.size(), location.path) != 0)
            continue;

        bool validBoundary = location.path == "/" || path.size() == location.path.size() || location.path[location.path.size() - 1] == '/'
            || path[location.path.size()] == '/';

        if (!validBoundary)
            continue;

        if (location.path.size() > bestLength)
        {
            bestMatch = &location;
            bestLength = location.path.size();
        }
    }

    return bestMatch;
}

bool HttpHandler::isMethodAllowed(const std::string &method, const Location *location) const
{
    if (!location)
        return method == "GET";

    if (location->methods.empty())
        return true;

    for (size_t i = 0; i < location->methods.size(); ++i)
    {
        if (toUpper(location->methods[i]) == method)
            return true;
    }

    return false;
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
    match.location = matchLocation(match.requestPath);

    if (!match.location)
    {
        match.root = serverConfig->root;
        match.fullPath = joinPath(match.root, match.requestPath);
        return;
    }

    match.root = match.location->root;

    if (match.root.empty()) match.root = serverConfig->root;

    std::string relativePath = match.requestPath;
    if (match.location->path != "/")
    {
        relativePath = match.requestPath.substr(match.location->path.size());

        if (relativePath.empty())
            relativePath = "/";

        else if (relativePath[0] != '/')
            relativePath = "/" + relativePath;
    }
    match.fullPath = joinPath(match.root, relativePath);
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

bool HttpHandler::resolveDirectory(RouteMatch &match, const std::string &method, HttpResponse &response) const
{
    if (!isDirectory(match.fullPath))
        return true;
    std::vector<std::string> indexes = resolveIndexFiles(match.location);

    for (size_t i = 0; i < indexes.size(); ++i)
    {
        std::string candidate =
            joinPath(match.fullPath, indexes[i]);

        if (!fileExists(candidate))
            continue;

        match.requestPath = joinPath(match.requestPath, indexes[i]);
        match.fullPath = candidate;
        return true;
    }
    bool autoIndexEnabled = match.location && match.location->autoindex == "on";

    if (method == "GET" && autoIndexEnabled)
    {
        response = resolveAutoIndexing(match, *serverConfig);
        return false;
    }
    response = ErrorPage(403, *serverConfig);
    return false;
}

HttpResult HttpHandler::process(const HttpRequest &request) const
{
    RouteMatch match;
    resolveRoute(request, match);

    if (match.location && match.location->redirectCode != 0)
    {
        return HttpResult::makeResponse(resolveRedirection(*match.location));
    }
    const std::string &method = request.getMethod();

    if (!isMethodAllowed(method, match.location))
    {
        return HttpResult::makeResponse(ErrorPage(405, *serverConfig));
    }
    if (request.getBody().size() > static_cast<size_t>(serverConfig->client_max_body_size))
    {
        return HttpResult::makeResponse(ErrorPage(413, *serverConfig));
    }
    HttpResponse directoryResponse;

    if (!resolveDirectory(match, method, directoryResponse))
        return HttpResult::makeResponse(directoryResponse);

    if (isCgiRequest(match))
    {
        return HttpResult::makeCgi(match.location, match.fullPath);
    }

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

    return HttpResult::makeResponse(
        ErrorPage(501, *serverConfig)
    );
}
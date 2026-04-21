#include "HttpHandler.hpp"

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

HttpResponse HttpHandler::buildError(int statusCode, const std::string& statusReason, const std::string& detail)
{
    HttpResponse response(statusCode, statusReason);
    response.setBody(detail, "text/plain");
    return response;
}


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

HttpResponse HttpHandler::handle404()
{
    std::string errorPageContent;
    std::string errorPath = Config->root + "/Error.html"; 
    
    if (HttpUtils::readFile(errorPath, errorPageContent))
    {
        HttpResponse response(404, "Not Found");
        response.setBody(errorPageContent, HttpUtils::contentType(errorPath));
        return response;
    }
    return buildError(404, "Not Found", "File not found");
}


HttpResponse HttpHandler::process(const HttpRequest& request)
{
    int errorCode = request.getErrorCode();
    if (errorCode != 0)
    {
        if (errorCode == 505) return buildError(505, "HTTP Version Not Supported", "Unsupported HTTP version");
        if (errorCode == 400) return buildError(400, "Bad Request", "Malformed request");
        return buildError(errorCode, "Error", "Parsing failed");
    }

    std::string method = request.getMethod();
    if (method != "GET" && method != "POST" && method != "DELETE")
        return buildError(405, "Method Not Allowed", "Unsupported method");

    std::string requestPath = HttpUtils::stripQuery(request.getTarget());

    const Location* matchedLocation = matchLocation(requestPath);
    if (!matchedLocation)
        return handle404();

    if (!isMethodAllowed(method, *matchedLocation))
        return buildError(405, "Method Not Allowed", "Method not allowed for route");

    if (method == "GET")
        return GET(request, requestPath);

    HttpResponse response(200, "OK");
    response.setBody("Hello, World!", "text/plain");
    return response;
}


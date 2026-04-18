// HttpHandler.cpp
#include "HttpHandler.hpp"
#include <fstream>
#include <cctype>

HttpHandler::HttpHandler() {}
HttpHandler::~HttpHandler() {}

static std::string toUpper(std::string value)
{
    for (size_t i = 0; i < value.size(); ++i)
        value[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(value[i])));
    return value;
}

static std::string toLower(std::string value)
{
    for (size_t i = 0; i < value.size(); ++i)
        value[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(value[i])));
    return value;
}

std::string HttpHandler::detectContentType(const std::string& path)
{
    size_t dot = path.find_last_of('.');
    if (dot == std::string::npos)
        return "application/octet-stream";
    std::string ext = toLower(path.substr(dot));
    if (ext == ".html" || ext == ".htm") return "text/html";
    if (ext == ".css") return "text/css";
    if (ext == ".js") return "application/javascript";
    if (ext == ".json") return "application/json";
    if (ext == ".txt") return "text/plain";
    if (ext == ".png") return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".gif") return "image/gif";
    return "application/octet-stream";
}

bool HttpHandler::readFile(const std::string& filePath, std::string& content)
{
    std::ifstream file(filePath.c_str(), std::ios::in | std::ios::binary);
    if (!file.is_open())
        return false;
    std::ostringstream data;
    data << file.rdbuf();
    content = data.str();
    return true;
}

HttpResponse HttpHandler::buildError(int code, const std::string& reason, const std::string& detail)
{
    HttpResponse res(code, reason);
    res.setBody(detail, "text/plain");
    return res;
}

HttpResponse HttpHandler::process(const HttpRequest& req, const ServerConfig& config)
{
    if (req.getErrorCode() != 0)
    {
        if (req.getErrorCode() == 505) return buildError(505, "HTTP Version Not Supported", "Unsupported HTTP version");
        if (req.getErrorCode() == 400) return buildError(400, "Bad Request", "Malformed request");
        return buildError(req.getErrorCode(), "Error", "Parsing failed");
    }

    if (req.getMethod() != "GET" && req.getMethod() != "POST" && req.getMethod() != "DELETE")
        return buildError(405, "Method Not Allowed", "Unsupported method");
    std::string route = req.getTarget();
    size_t query = route.find('?');
    if (query != std::string::npos)
        route = route.substr(0, query);

    const Location* matchedLocation = NULL;
    size_t bestLen = 0;
    for (size_t i = 0; i < config.Locations.size(); ++i)
    {
        const Location& current = config.Locations[i];
        if (route.compare(0, current.path.size(), current.path) == 0 && current.path.size() >= bestLen)
        {
            matchedLocation = &current;
            bestLen = current.path.size();
        }
    }

    if (!matchedLocation || (matchedLocation->path == "/" && route != "/"))
    {
        std::string fileContent;
        if (readFile("./Error.html", fileContent))
        {
            HttpResponse res(404, "Not Found");
            res.setBody(fileContent, detectContentType("./Error.html"));
            return res;
        }
        return buildError(404, "Not Found", "File not found");
    }

    if (!matchedLocation->methods.empty())
    {
        bool allowed = false;
        for (size_t i = 0; i < matchedLocation->methods.size(); ++i)
        {
            if (toUpper(matchedLocation->methods[i]) == req.getMethod())
            {
                allowed = true;
                break;
            }
        }
        if (!allowed)
            return buildError(405, "Method Not Allowed", "Method not allowed for route");
    }

    if (req.getMethod() == "GET")
        return handleGet(req, route, config);

    HttpResponse res(200, "OK");
    res.setBody("Hello, World!", "text/plain");
    return res;
}

HttpResponse HttpHandler::handleGet(const HttpRequest& req, std::string route, const ServerConfig& config)
{
    (void)req;
    if (route.empty() || route == "/")
        route = "/index.html";
    std::string filePath = config.root + route;
    std::string fileContent;

    bool found = readFile(filePath, fileContent);
    if (!found && route == "/index.html")
    {
        filePath = "./index.html";
        found = readFile(filePath, fileContent);
    }

    if (found)
    {
        HttpResponse res(200, "OK");
        res.setBody(fileContent, detectContentType(filePath));
        return res;
    }
    return buildError(404, "Not Found", "File not found");
}
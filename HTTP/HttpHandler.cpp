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
    size_t extensionPos = path.find_last_of('.');
    if (extensionPos == std::string::npos)
        return "application/octet-stream";

    std::string fileExtension = toLower(path.substr(extensionPos));

    if (fileExtension == ".html" || fileExtension == ".htm") return "text/html";
    if (fileExtension == ".css") return "text/css";
    if (fileExtension == ".js") return "application/javascript";
    if (fileExtension == ".json") return "application/json";
    if (fileExtension == ".txt") return "text/plain";
    if (fileExtension == ".png") return "image/png";
    if (fileExtension == ".jpg" || fileExtension == ".jpeg") return "image/jpeg";
    if (fileExtension == ".gif") return "image/gif";

    return "application/octet-stream";
}

bool HttpHandler::readFile(const std::string& filePath, std::string& content)
{
    std::ifstream file(filePath.c_str(), std::ios::in | std::ios::binary);
    if (!file.is_open())
        return false;

    std::ostringstream fileBuffer;
    fileBuffer << file.rdbuf();
    content = fileBuffer.str();
    return true;
}

HttpResponse HttpHandler::buildError(int statusCode, const std::string& statusReason, const std::string& detail)
{
    HttpResponse response(statusCode, statusReason);
    response.setBody(detail, "text/plain");
    return response;
}

HttpResponse HttpHandler::process(const HttpRequest& request, const ServerConfig& config)
{

    int errorCode = request.getErrorCode();
    if (errorCode != 0)
    {
        if (errorCode == 505) 
            return buildError(505, "HTTP Version Not Supported", "Unsupported HTTP version");
        if (errorCode == 400)
            return buildError(400, "Bad Request", "Malformed request");
        return buildError(errorCode, "Error", "Parsing failed");
    }


    std::string method = request.getMethod();
    if (method != "GET" && method != "POST" && method != "DELETE")
        return buildError(405, "Method Not Allowed", "Unsupported method");

    std::string requestPath = request.getTarget();
    size_t queryStartPos = requestPath.find('?');
    if (queryStartPos != std::string::npos)
        requestPath = requestPath.substr(0, queryStartPos);

    const Location *bestLocationMatch = NULL;
    size_t longestMatchLen = 0;

    for (size_t i = 0; i < config.Locations.size(); ++i)
    {
        const Location &candidateLocation = config.Locations[i];
        
        bool isPrefixMatch = (requestPath.compare(0, candidateLocation.path.size(), candidateLocation.path) == 0);
        if (isPrefixMatch && candidateLocation.path.size() >= longestMatchLen)
        {
            bestLocationMatch = &candidateLocation;
            longestMatchLen = candidateLocation.path.size();
        }
    }

    if (!bestLocationMatch || (bestLocationMatch->path == "/" && requestPath != "/"))
    {
        std::string errorPageContent;
        if (readFile("./Error.html", errorPageContent))
        {
            HttpResponse response(404, "Not Found");
            response.setBody(errorPageContent, detectContentType("./Error.html"));
            return response;
        }
        return buildError(404, "Not Found", "File not found");
    }

    if (!bestLocationMatch->methods.empty())
    {
        bool isAllowed = false;
        for (size_t i = 0; i < bestLocationMatch->methods.size(); ++i)
        {
            if (toUpper(bestLocationMatch->methods[i]) == method)
            {
                isAllowed = true;
                break;
            }
        }
        if (!isAllowed)
            return buildError(405, "Method Not Allowed", "Method not allowed for route");
    }
    if (method == "GET")
        return handleGet(request, requestPath, config);

    HttpResponse response(200, "OK");
    response.setBody("Hello, World!", "text/plain");
    return response;
}

HttpResponse HttpHandler::handleGet(const HttpRequest& request, std::string requestPath, const ServerConfig& config)
{
    (void)request; 

    if (requestPath.empty() || requestPath == "/")
        requestPath = "/index.html";

    std::string fullPath = config.root + requestPath;
    std::string fileContent;

    bool isFound = readFile(fullPath, fileContent);

    if (!isFound && requestPath == "/index.html")
    {
        fullPath = "./index.html";
        isFound = readFile(fullPath, fileContent);
    }
    if (isFound)
    {
        HttpResponse response(200, "OK");
        response.setBody(fileContent, detectContentType(fullPath));
        return response;
    }
    return buildError(404, "Not Found", "File not found");
}
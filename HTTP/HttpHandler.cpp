
#include "HttpHandler.hpp"


HttpHandler::HttpHandler() {}
HttpHandler::~HttpHandler() {}

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

std::string HttpHandler::stripQuery(const std::string& path)
{
    size_t queryStart = path.find('?');
    if (queryStart != std::string::npos)
        return path.substr(0, queryStart);
    return path;
}

const Location* HttpHandler::matchLocation(const std::string& path, const ServerConfig& config)
{
    const Location* bestMatch = NULL;
    size_t longestLen = 0;

    for (size_t i = 0; i < config.Locations.size(); ++i)
    {
        const Location& loc = config.Locations[i];
        if (path.compare(0, loc.path.size(), loc.path) == 0 && loc.path.size() >= longestLen)
        {
            bestMatch = &loc;
            longestLen = loc.path.size();
        }
    }
    return bestMatch;
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


HttpResponse HttpHandler::handle404(const ServerConfig& config)
{
    std::string errorPageContent;
    std::string errorPath = config.root + "/Error.html"; 
    
    if (readFile(errorPath, errorPageContent))
    {
        HttpResponse response(404, "Not Found");
        response.setBody(errorPageContent, detectContentType(errorPath));
        return response;
    }
    return buildError(404, "Not Found", "File not found");
}

HttpResponse HttpHandler::process(const HttpRequest& request, const ServerConfig& config)
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

    std::string requestPath = stripQuery(request.getTarget());

    const Location* matchedLocation = matchLocation(requestPath, config);
    if (!matchedLocation)
        return handle404(config);

    if (!isMethodAllowed(method, *matchedLocation))
        return buildError(405, "Method Not Allowed", "Method not allowed for route");

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
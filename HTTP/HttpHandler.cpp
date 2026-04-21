#include "HttpHandler.hpp"
#include "../CGI/CGI.hpp"

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

HttpResponse HttpHandler::formatCgiResponse(const std::string& cgiOutput)
{
    HttpResponse response(200, "OK");
    std::string contentType = "text/html";
    
    size_t delimiter = cgiOutput.find("\n\n");
    if (delimiter == std::string::npos)
        delimiter = cgiOutput.find("\r\n\r\n");

    if (delimiter == std::string::npos) {
        response.setBody(cgiOutput, contentType);
        return response;
    }

    std::string headersPart = cgiOutput.substr(0, delimiter);
    std::string bodyPart = cgiOutput.substr(delimiter + (cgiOutput[delimiter+1] == '\r' ? 4 : 2));

    std::stringstream ss(headersPart);
    std::string line;
    while (std::getline(ss, line)) {
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);

        if (line.find("Content-Type: ") == 0) {
            contentType = line.substr(14);
        }
        else if (line.find("Status: ") == 0) {
            // later
        }
    }

    response.setBody(bodyPart, contentType);
    return response;
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

    if (!matchedLocation->cgiExt.empty())
    {
        if (requestPath.size() >= matchedLocation->cgiExt.size() &&
            requestPath.compare(requestPath.size() - matchedLocation->cgiExt.size(), 
                                matchedLocation->cgiExt.size(), matchedLocation->cgiExt) == 0)
        {
            CgiHandler cgi;
            try
            {
                std::string cgiOutput = cgi.execute(request, *matchedLocation, requestPath);
                return formatCgiResponse(cgiOutput);
            }
            catch (const std::exception& e)
            {
                return buildError(500, "Internal Server Error", "CGI Execution Failed");
            }
        }
    }

    if (!isMethodAllowed(method, *matchedLocation))
        return buildError(405, "Method Not Allowed", "Method not allowed for route");

    if (method == "GET")
        return GET(request, requestPath);

    HttpResponse response(200, "OK");
    response.setBody("Hello, World!", "text/plain");
    return response;
}

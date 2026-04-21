#include "HttpHandler.hpp"

HttpResponse HttpHandler::GET(const HttpRequest& request, std::string requestPath)
{
    (void)request; 

    if (requestPath.empty() || requestPath == "/")
        requestPath = "/index.html";

    std::string fullPath = Config->root + requestPath;
    std::string fileContent;

    bool isFound = HttpUtils::readFile(fullPath, fileContent);

    if (!isFound && requestPath == "/index.html")
    {
        fullPath = "./index.html";
        isFound = HttpUtils::readFile(fullPath, fileContent);
    }
    if (isFound)
    {
        HttpResponse response(200, "OK");
        response.setBody(fileContent, HttpUtils::contentType(fullPath));
        return response;
    }
    return handle404();
}
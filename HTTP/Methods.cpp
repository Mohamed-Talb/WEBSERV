#include "Methods.hpp"
#include "HttpUtils.hpp"

HttpResponse HttpMethods::GET(const std::string &rootDirectory, std::string requestPath, const ServerConfig &config)
{ 
    if (requestPath.empty() || requestPath == "/")
        requestPath = "/index.html";
        
    std::string fullPath = rootDirectory + requestPath;
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
       return HttpUtils::ErrorPage(404, "Not Found", config);
} 


// void HttpHandler::DELETE(const HttpRequest &request, std::string requestPath)
// {
//     std::cout << requestPath << std::endl;
//     (void)request;
// }
#include "Methods.hpp"
#include "HttpUtils.hpp"


HttpResponse HttpMethods::GET(const std::string &rootDirectory,
                              std::string requestPath,
                              const ServerConfig &config)
{
    std::string fullPath = joinPath(rootDirectory, requestPath);
    std::string fileContent;

    if (FileSystem::readFile(fullPath, fileContent))
    {
        HttpResponse response(200, "OK");
        response.setBody(fileContent, HttpUtils::contentType(fullPath));
        return response;
    }

    return HttpUtils::ErrorPage(404, "Not Found", config);
}


#include "Methods.hpp"
#include "HttpUtils.hpp"


HttpResponse HttpMethods::DELETE(const std::string &rootDirectory, std::string requestPath, const ServerConfig &config)
{
    if (requestPath.find("..") != std::string::npos)
        return HttpUtils::ErrorPage(403, "Forbidden", config);
    std::string fullPath = rootDirectory + requestPath;
    if (!FileSystem::fileExists(fullPath)) 
    {
        return HttpUtils::ErrorPage(404, "Not Found", config);
    }
    if (!FileSystem::deleteFile(fullPath)) 
    {
        return HttpUtils::ErrorPage(403, "Forbidden", config);
    }
    return HttpResponse(204, "No Content");
}
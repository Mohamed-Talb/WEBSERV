#include "Methods.hpp"
#include "HttpUtils.hpp"

HttpResponse HttpMethods::GET(RouteMatch* match, const ServerConfig& config)
{
    if (!match)
        return HttpUtils::ErrorPage(500, "Internal Server Error", config);

    std::string fileContent;

    if (FileSystem::readFile(match->fullPath, fileContent))
    {
        HttpResponse response(200, "OK");
        response.setBody(fileContent);
        response.setHeader("Content-Type", HttpUtils::contentType(match->fullPath));
        return response;
    }

    return HttpUtils::ErrorPage(404, "Not Found", config);
}


HttpResponse HttpMethods::DELETE(RouteMatch* match, const ServerConfig& config)
{
    if (!match)
        return HttpUtils::ErrorPage(500, "Internal Server Error", config);

    if (match->requestPath.find("..") != std::string::npos)
        return HttpUtils::ErrorPage(403, "Forbidden", config);

    if (!FileSystem::fileExists(match->fullPath))
        return HttpUtils::ErrorPage(404, "Not Found", config);

    if (!FileSystem::deleteFile(match->fullPath))
        return HttpUtils::ErrorPage(403, "Forbidden", config);

    return HttpResponse(204, "No Content");
}


HttpResponse HttpMethods::POST(const HttpRequest &request, RouteMatch *match, const ServerConfig &config)
{
    if (!match || !match->location)
        return HttpUtils::ErrorPage(500, "Internal Server Error!", config);

    if (match->location->uploadEnabled != "on")
        return HttpUtils::ErrorPage(403, "Forbidden", config);

    if (match->location->uploadPath.empty())
        return HttpUtils::ErrorPage(500, "Internal Server Error2", config);

    if (!HttpUtils::isDirectory(match->location->uploadPath))
        return HttpUtils::ErrorPage(500, match->location->uploadPath, config);

    std::string contentType = request.getHeader("content-type");

    if (contentType.find("multipart/form-data") == std::string::npos)
        return HttpUtils::ErrorPage(415, "Unsupported Media Type", config);

    std::string boundary;
    if (!extractBoundary(contentType, boundary))
        return HttpUtils::ErrorPage(400, "Bad Request", config);

    std::string filename;
    std::string fileContent;

    if (!parseMultipartFile(request.getBody(), boundary, filename, fileContent))
        return HttpUtils::ErrorPage(400, "Bad Request", config);

    std::string outputPath = joinPath(match->location->uploadPath, filename);

    if (!FileSystem::writeToFile(outputPath, fileContent))
        return HttpUtils::ErrorPage(500, "Internal Server Error4", config);

    HttpResponse response(201, "Created");
    response.setBody("File uploaded successfully\n");
    response.setHeader("Content-Type", "text/plain");
    return response;
}
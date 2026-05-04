#include "Methods.hpp"

HttpResponse HttpMethods::GET(const RouteMatch &match, const ServerConfig &config)
{
    std::string fileContent;

    if (readFile(match.fullPath, fileContent))
    {
        HttpResponse response(200, "OK");
        response.setBody(fileContent);
        response.setHeader("Content-Type", contentType(match.fullPath));
        return response;
    }

    return ErrorPage(404, "Not Found", config);
}


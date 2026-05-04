#include "Methods.hpp"

HttpResponse HttpMethods::DELETE(const RouteMatch &match, const ServerConfig& config)
{
    if (match.requestPath.find("..") != std::string::npos)
        return ErrorPage(403, "Forbidden", config);

    if (!fileExists(match.fullPath))
        return ErrorPage(404, "Not Found", config);

    if (!deleteFile(match.fullPath))
        return ErrorPage(403, "Forbidden", config);

    return HttpResponse(204, "No Content");
}

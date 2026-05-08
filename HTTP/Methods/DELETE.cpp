#include "Methods.hpp"

HttpResponse HttpMethods::DELETE(const RouteMatch &match, const ServerConfig& config)
{
    if (!fileExists(match.fullPath))
        return ErrorPage(404, config);

    if (!deleteFile(match.fullPath))
        return ErrorPage(403, config);

    return HttpResponse(204, "No Content");
}

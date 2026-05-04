#ifndef HTTPMETHODS_HPP
#define HTTPMETHODS_HPP

#include "../HttpUtils/HttpUtils.hpp"
#include "../../FileSystem.hpp"
#include "../../Helpers.hpp"
#include "../RouteMatch.hpp"
#include "../HttpResponse.hpp"
#include "../HttpRequest.hpp"

class HttpMethods 
{
    public:
    static HttpResponse GET(const RouteMatch &match, const ServerConfig &config);
    static HttpResponse DELETE(const RouteMatch &match, const ServerConfig &config);
    static HttpResponse POST(const HttpRequest &request, const RouteMatch &match, const ServerConfig &config);
};

#endif
#ifndef HTTPMETHODS_HPP
#define HTTPMETHODS_HPP

#include "HttpHandler.hpp"
#include <string>

class HttpMethods 
{
    public:
    static HttpResponse GET(RouteMatch *match, const ServerConfig &config);
    static HttpResponse DELETE(RouteMatch *match, const ServerConfig &config);
    static HttpResponse POST(const HttpRequest& request, RouteMatch *match, const ServerConfig &config);

    // static HttpResponse DELETE(const std::string& rootDirectory, const std::string& targetPath);
    // static HttpResponse POST(const std::string& rootDirectory, const HttpRequest& request); 
};

#endif
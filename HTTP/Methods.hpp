#ifndef HTTPMETHODS_HPP
#define HTTPMETHODS_HPP

#include "HttpHandler.hpp"
#include <string>

class HttpMethods 
{
    public:
    static HttpResponse GET(const std::string &rootDirectory, std::string requestPath, const ServerConfig &config);
    // static HttpResponse DELETE(const std::string& rootDirectory, const std::string& targetPath);
    // static HttpResponse POST(const std::string& rootDirectory, const HttpRequest& request); 
};

#endif
// HttpHandler.hpp
#ifndef HTTPHANDLER_HPP
#define HTTPHANDLER_HPP

#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "../configParser/configParser.hpp"
#include <string>

class HttpHandler
{
private:
    std::string detectContentType(const std::string& path);
    bool readFile(const std::string& filePath, std::string& content);
    HttpResponse handleGet(const HttpRequest& req, std::string route, const ServerConfig& config);

public:
    HttpHandler();
    ~HttpHandler();

    HttpResponse process(const HttpRequest& req, const ServerConfig& config);
    HttpResponse buildError(int code, const std::string& reason, const std::string& detail);
};

#endif
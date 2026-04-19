
#ifndef HTTPHANDLER_HPP
#define HTTPHANDLER_HPP

#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "../configParser/configParser.hpp"
#include <string>
#include <fstream>
#include <cctype>
#include "../Helpers.hpp"
class HttpHandler
{
    private:
    std::string detectContentType(const std::string& path);
    bool readFile(const std::string& filePath, std::string& content);
    HttpResponse handleGet(const HttpRequest& req, std::string route, const ServerConfig& config);
    std::string     stripQuery(const std::string& path);
    const Location* matchLocation(const std::string& path, const ServerConfig& config);
    bool            isMethodAllowed(const std::string& method, const Location& loc);
    HttpResponse    handle404(const ServerConfig& config);
    HttpResponse buildError(int code, const std::string& reason, const std::string& detail);
    public:
    HttpHandler();
    ~HttpHandler();

    HttpResponse process(const HttpRequest& req, const ServerConfig& config);
};

#endif
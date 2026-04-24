#ifndef HTTPUTILS_HPP
#define HTTPUTILS_HPP
#include "HttpHandler.hpp"
namespace HttpUtils
{
    std::string contentType(const std::string &path);
    std::string stripQuery(const std::string &path);
    bool        readFile(const std::string &filePath, std::string& content);
    HttpResponse ErrorPage(int statusCode, const std::string &statusReason, const ServerConfig &config);
}

#endif
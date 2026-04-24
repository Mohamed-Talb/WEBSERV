#ifndef HTTPUTILS_HPP
#define HTTPUTILS_HPP
#include "HttpHandler.hpp"
namespace HttpUtils
{
    std::string contentType(const std::string &path);
    std::string stripQuery(const std::string &path);
    HttpResponse ErrorPage(int statusCode, const std::string &statusReason, const ServerConfig &config);
}

#endif
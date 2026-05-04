#ifndef HTTPUTILS_HPP
#define HTTPUTILS_HPP

#include <map>
#include <string>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include "../../Helpers.hpp"
#include "../HttpHandler.hpp"
#include "../HttpResponse.hpp"
#include "../RouteMatch.hpp"
#include "../../configParser/config.hpp"



std::string contentType(const std::string &path);
std::string stripQuery(const std::string &path);
HttpResponse ErrorPage(int statusCode, const std::string &statusReason, const ServerConfig &config);
HttpResponse resolveAutoIndexing(const RouteMatch &match, const ServerConfig &serverConfig);
#endif
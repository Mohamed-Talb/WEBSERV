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
#include "../../configParser/configParser.hpp"



std::string contentType(const std::string &path);
HttpResponse ErrorPage(int statusCode, const ServerConfig &config);
HttpResponse resolveAutoIndexing(const RouteMatch &match, const ServerConfig &serverConfig);
std::string normalizePath(const std::string &path);

bool parseHexSize(const std::string &value, size_t &result);
bool parseDecimalSize(const std::string &value, size_t &result);
bool urlDecode(const std::string &input, std::string &output);
bool isCommaSeparatedHeader(const std::string &name);
std::vector<std::string> splitHeaderValues(const std::string &value);
const Location *matchLocation(const ServerConfig &config, const std::string &path);
const ServerConfig *matchConfig(const std::vector<ServerConfig *> &configs, const std::string &rawHost);

#endif
#ifndef HTTPHANDLER_HPP
#define HTTPHANDLER_HPP

#include "../configParser/configParser.hpp"
#include <string>
#include <fstream>
#include <cctype>
#include "../Helpers.hpp"
#include "algorithm"
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"

#include <string>
#include <map>
#include <sstream>


struct RouteMatch
{
    const Location *location;
    std::string root;
    std::string fullPath;
    std::string requestPath;
};

class HttpHandler 
{
	private:
    const ServerConfig 	*serverConfig;
    const Location *matchLocation(const std::string &path);
    bool isMethodAllowed(const std::string& method, const Location& loc);
    void resolveRoute(const HttpRequest& request, RouteMatch& match);    // METHODS
    public:
	std::vector<std::string> resolveIndexFiles(const Location *loc);
	const Location *getCgiLocation(const HttpRequest &request);
    HttpHandler(const ServerConfig &serverConfig);
    ~HttpHandler();

    HttpResponse process(const HttpRequest& req);
};

HttpResponse resolveAutoIndex(const RouteMatch &match, const ServerConfig *serverConfig);
HttpResponse generateDirectoryListing(const RouteMatch &match, const ServerConfig *serverConfig);
bool parseMultipartFile(const std::string &body, const std::string &boundary, std::string &filename, std::string &fileContent);
bool extractBoundary(const std::string &contentType, std::string &boundary);

#endif
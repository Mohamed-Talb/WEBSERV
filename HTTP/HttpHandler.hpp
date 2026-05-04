#ifndef HTTPHANDLER_HPP
#define HTTPHANDLER_HPP

#include "../CGI/CGI.hpp"
#include "Methods/Methods.hpp" 
#include "HttpUtils/HttpUtils.hpp"

#include "../FileSystem.hpp"
#include "../Helpers.hpp"
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "../configParser/config.hpp"
#include "RouteMatch.hpp"

#include <vector>
#include <string>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <dirent.h>
#include <algorithm>
#include <sys/stat.h>

class HttpHandler 
{
	private:
    const ServerConfig 	*serverConfig;
    const Location *matchLocation(const std::string &path);
    void resolveRoute(const HttpRequest& request, RouteMatch &match); 
    bool isMethodAllowed(const std::string& method, const Location &loc);
    
    public:
    ~HttpHandler();
    HttpResponse process(const HttpRequest &req);
    HttpHandler(const ServerConfig &serverConfig);
	const Location *getCgiLocation(const HttpRequest &request);
	std::vector<std::string> resolveIndexFiles(const Location *loc);

};

#endif
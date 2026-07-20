#ifndef HTTPHANDLER_HPP
#define HTTPHANDLER_HPP

#include "../CGI/CGI.hpp"
#include "Methods/Methods.hpp" 
#include "HttpUtils/HttpUtils.hpp"

#include "../FileSystem.hpp"
#include "../Helpers.hpp"
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "../configParser/configParser.hpp"
#include "RouteMatch.hpp"

#include <vector>
#include <string>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <dirent.h>
#include <algorithm>
#include <sys/stat.h>

#include "HttpResponse.hpp"
#include "RouteMatch.hpp"
#include "../configParser/configParser.hpp"
#include <string>

enum HttpResultType
{
    HTTP_RESULT_RESPONSE,
    HTTP_RESULT_CGI
};

struct HttpResult
{
    HttpResultType type;
    HttpResponse response;
    const Location *cgiLocation;
    std::string cgiRequestPath;

    HttpResult()
        : type(HTTP_RESULT_RESPONSE), response(),
        cgiLocation(NULL), cgiRequestPath() {}

    static HttpResult makeResponse(const HttpResponse &res)
    {
        HttpResult result;
        result.type = HTTP_RESULT_RESPONSE;
        result.response = res;
        return result;
    }

    static HttpResult makeCgi(const Location *loc, const std::string &path)
    {
        HttpResult result;
        result.type = HTTP_RESULT_CGI;
        result.cgiLocation = loc;
        result.cgiRequestPath = path;
        return result;
    }
};



class HttpHandler
{
    private:
    const ServerConfig *serverConfig;

    const Location *matchLocation(const std::string &path) const;
    bool isMethodAllowed(const Location *location, const std::string &method) const;
    bool isCgiRequest(const RouteMatch &match) const;
    void resolveRoute(const HttpRequest &request,RouteMatch &match) const;
    std::vector<std::string> resolveIndexFiles(const Location *location) const;
    bool resolveDirectory(RouteMatch &match, const HttpRequest request, HttpResponse &response) const;
    HttpResponse resolveRedirection(const Location &location) const;
    HttpResult resolveCgi(const RouteMatch &match) const;
    
    public:
    HttpHandler(const ServerConfig &config);
    HttpResult process(const HttpRequest &request) const;
};

#endif
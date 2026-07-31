#ifndef HTTPHANDLER_HPP
#define HTTPHANDLER_HPP


#include <vector>
#include <string>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <dirent.h>
#include <algorithm>
#include <sys/stat.h>

#include "RouteMatch.hpp"
#include "../Helpers.hpp"
#include "../CGI/CGI.hpp"
#include "HttpResponse.hpp"
#include "../FileSystem.hpp"
#include "Methods/Methods.hpp" 
#include "HttpUtils/HttpUtils.hpp"
#include "./HttpRequest/HttpRequest.hpp"
#include "../configParser/configParser.hpp"


enum HttpResultType
{
    HTTP_RESULT_RESPONSE,
    HTTP_RESULT_CGI
};

struct HttpResult
{
    HttpResultType type;
    HttpResponse response;
    std::string cgiRequestPath;
    std::string cgiInterpreter;
    const Location *cgiLocation;
    
    HttpResult() : type(HTTP_RESULT_RESPONSE), response(), cgiRequestPath(), cgiInterpreter(), cgiLocation(NULL) {}
    static HttpResult makeResponse(const HttpResponse &res)
    {
        HttpResult result;
        result.type = HTTP_RESULT_RESPONSE;
        result.response = res;
        return result;
    }
    static HttpResult makeCgi(const Location *loc, const std::string &path, const std::string &interpreter)
    {
        HttpResult result;
        result.type = HTTP_RESULT_CGI;
        result.cgiLocation = loc;
        result.cgiRequestPath = path;
        result.cgiInterpreter = interpreter;
        return result;
    }
};



class HttpHandler
{
    private:
    const ServerConfig *serverConfig;

    bool isCgiRequest(const RouteMatch &match) const;
    HttpResponse resolveRedirection(const Location &location) const;
    void resolveRoute(const HttpRequest &request,RouteMatch &match) const;
    std::vector<std::string> resolveIndexFiles(const Location *location) const;
    bool isMethodAllowed(const Location *location, const std::string &method) const;
    bool resolveDirectory(RouteMatch &match, const HttpRequest &request, HttpResponse &response) const;
    
    public:
    HttpHandler(const ServerConfig &config);
    HttpResult process(const HttpRequest &request) const;
};

#endif

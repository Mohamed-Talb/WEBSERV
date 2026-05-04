#include "HttpUtils.hpp"

HttpResponse ErrorPage(int statusCode, const std::string &statusReason, const ServerConfig &config)
{
    std::string errorPageContent;
    std::string errorPath;
    std::map<int, std::string>::const_iterator it = config.errorPage.find(statusCode);
    
    if (it != config.errorPage.end()) 
    {
        errorPath = config.root + it->second;
    }
    if (!errorPath.empty() && readFile(errorPath, errorPageContent))
    {
        HttpResponse response(statusCode, statusReason);
        response.setBody(errorPageContent);
        response.setHeader("Content-Type", contentType(errorPath));
        return response;
    }
    std::ostringstream defaultHtml;
    defaultHtml << "<html><head><title>" << statusCode << " " << statusReason << "</title></head>"
                << "<body><center><h1>" << statusCode << " " << statusReason << "</h1></center>"
                << "<hr><center>webserv/1.0</center></body></html>";

    HttpResponse response(statusCode, statusReason);
    response.setBody(defaultHtml.str());
    response.setHeader("Content-Type","text/html");
    return response;
}

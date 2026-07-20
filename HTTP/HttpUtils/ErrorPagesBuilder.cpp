#include "HttpUtils.hpp"


std::string getReasonPhrase(int statusCode)
{
    switch (statusCode)
    {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 303: return "See Other";
        case 307: return "Temporary Redirect";
        case 308: return "Permanent Redirect";
        case 400: return "Bad Request";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 413: return "Payload Too Large";
        case 414: return "URI Too Long";
        case 431: return "Request Header Fields Too Large";
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        case 505: return "HTTP Version Not Supported";
        default:  return "Unknown Error";
    }
}

HttpResponse ErrorPage(int statusCode, const ServerConfig &config)
{

    std::string statusReason = getReasonPhrase(statusCode);
    
    std::string errorPageContent;
    std::string errorPath;
    std::map<int, std::string>::const_iterator it = config.errorPage.find(statusCode);
    
    if (it != config.errorPage.end()) 
    {
        errorPath = joinPath(config.root, it->second); // Safer to use joinPath here!
    }
    if (!errorPath.empty() && readFile(errorPath, errorPageContent))
    {
        HttpResponse response(statusCode, statusReason);
        response.setBody(errorPageContent);
        response.setHeader("Content-Type", contentType(errorPath));
        return response;
    }
    std::ostringstream defaultHtml;
    defaultHtml << "<html><head><title>" << statusCode << " " << statusReason << "</title></head>\n"
                << "<body><center><h1>" << statusCode << " " << statusReason << "</h1></center>\n"
                << "<hr><center>webserv/1.0</center></body></html>";

    HttpResponse response(statusCode, statusReason);
    response.setBody(defaultHtml.str());
    response.setHeader("Content-Type", "text/html");
    
    return response;
}
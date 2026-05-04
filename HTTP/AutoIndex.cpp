#include "HttpHandler.hpp"
#include "HttpUtils.hpp"
#include <sys/stat.h>
#include <dirent.h>
#include <sstream>
#include <iomanip>
#include <cctype>


static std::string urlEncode(const std::string &str)
{
    std::ostringstream out;
    for (size_t i = 0; i < str.size(); ++i)
    {
        unsigned char c = static_cast<unsigned char>(str[i]);
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
        {
            out << c;
        }
        else
        {
            out << '%' << std::uppercase << std::hex << std::setw(2)
                << std::setfill('0') << static_cast<int>(c) << std::nouppercase << std::dec;
        }
    }
    return out.str();
}

static std::string htmlEscaping(std::string &srcHtml)
{
    std::string newHtml;
    char currChar;
    for(size_t i = 0; i < srcHtml.length(); i++)
    {
        currChar = srcHtml[i];
        if(currChar == '&')
            newHtml += "&amp;";
        else if(currChar == '<')
            newHtml += "&lt;";
        else if(currChar == '>')
            newHtml += "&gt;";
        else if(currChar == '"')
            newHtml += "&quot;";
        else
            newHtml += currChar;
    }
    return newHtml;
}

HttpResponse resolveAutoIndex(const RouteMatch &match, const ServerConfig *serverConfig)
{
    DIR *dir = opendir(match.fullPath.c_str());

    if (dir == NULL)
        return HttpUtils::ErrorPage(403, "Forbidden", *serverConfig);

    HttpResponse response(200, "OK");
    response.setHeader("Content-Type", "text/html");

    std::string urlBase = match.requestPath;

    if (urlBase.empty())
        urlBase = "/";

    if (urlBase[urlBase.size() - 1] != '/')
        urlBase += "/";

    response.writeBody("<html>");
    response.writeBody("<head><title>Index of ");
    response.writeBody(htmlEscaping(urlBase));
    response.writeBody("</title></head>");
    response.writeBody("<body>");
    response.writeBody("<h1>Index of ");
    response.writeBody(htmlEscaping(urlBase));
    response.writeBody("</h1>");
    response.writeBody("<ul>");

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        std::string entryName = entry->d_name;
        if (entryName == "." || entryName == "..")
            continue;
        std::string entryFullPath = joinPath(match.fullPath, entryName);
        bool isDir = HttpUtils::isDirectory(entryFullPath);
        std::string hrefName = urlEncode(entryName);
        std::string displayName = htmlEscaping(entryName);
        if (isDir)
        {
            hrefName += "/";
            displayName += "/";
        }
        response.writeBody("<li><a href=\"");
        response.writeBody(urlBase + hrefName);
        response.writeBody("\">");
        response.writeBody(displayName);
        response.writeBody("</a></li>");
    }
    closedir(dir);
    response.writeBody("</ul>");
    response.writeBody("</body>");
    response.writeBody("</html>");
    return response;
}

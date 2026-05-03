#include "Methods.hpp"
#include "HttpUtils.hpp"

// std::string getInBetween(std::string str, std::string s1, std::string s2)
// {
//     std::string result = str.substr(str.find(s1) + s1.size());
//     result = result.substr(0, result.find(s2));
//     return result;
// }

// void storeFile(std::string body, std::string rootDirectory)
// {
//     std::string disp = getInBetween(body, "Content-Disposition: ", "\r\n");

//     std::cout << "trimedBody: " << body;
//     // dbg_print("trimedBody: ", body);
//     if (disp.find("filename=") != std::string::npos)
//     {
//         std::string filename = getInBetween(disp, "filename=\"", "\"");
//         std::string content = body.substr(body.find("\r\n\r\n") + 4);
//         FileSystem::writeToFile(rootDirectory + "/" + filename, content);
//     }
// }


HttpResponse HttpMethods::GET(RouteMatch* match, const ServerConfig& config)
{
    if (!match)
        return HttpUtils::ErrorPage(500, "Internal Server Error", config);

    std::string fileContent;

    if (FileSystem::readFile(match->fullPath, fileContent))
    {
        HttpResponse response(200, "OK");
        response.setBody(fileContent, HttpUtils::contentType(match->fullPath));
        return response;
    }

    return HttpUtils::ErrorPage(404, "Not Found", config);
}


HttpResponse HttpMethods::DELETE(RouteMatch* match, const ServerConfig& config)
{
    if (!match)
        return HttpUtils::ErrorPage(500, "Internal Server Error", config);

    if (match->requestPath.find("..") != std::string::npos)
        return HttpUtils::ErrorPage(403, "Forbidden", config);

    if (!FileSystem::fileExists(match->fullPath))
        return HttpUtils::ErrorPage(404, "Not Found", config);

    if (!FileSystem::deleteFile(match->fullPath))
        return HttpUtils::ErrorPage(403, "Forbidden", config);

    return HttpResponse(204, "No Content");
}

// static void dbg_print(std::string identifier, std::string data)
// {
//     std::cout << identifier << data << std::endl;
// }


// HttpResponse HttpMethods::POST(const HttpRequest &request, RouteMatch *match, const ServerConfig &config)
// {
//     (void) request;
//     (void) config;
    
//     std::string boundary = getInBetween(request.getHeader("content-type"), "boundary=", "\n");
//     // dbg_print("boundary is: ", boundary);
    
//     std::string trimedBody = getInBetween(request.getBody(), boundary, "--" + boundary);
//     storeFile(trimedBody, match->root);
//     return HttpResponse(200, "OK");
// }

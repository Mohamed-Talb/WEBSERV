#include "Methods.hpp"
#include "HttpUtils.hpp"


HttpResponse HttpMethods::GET(const std::string &rootDirectory,
                              std::string requestPath,
                              const ServerConfig &config)
{
    std::string fullPath = joinPath(rootDirectory, requestPath);
    std::string fileContent;

    if (FileSystem::readFile(fullPath, fileContent))
    {
        HttpResponse response(200, "OK");
        response.setBody(fileContent, HttpUtils::contentType(fullPath));
        return response;
    }

    return HttpUtils::ErrorPage(404, "Not Found", config);
}


HttpResponse HttpMethods::DELETE(const std::string &rootDirectory, std::string requestPath, const ServerConfig &config)
{
    if (requestPath.find("..") != std::string::npos)
        return HttpUtils::ErrorPage(403, "Forbidden", config);
    std::string fullPath = rootDirectory + requestPath;
    if (!FileSystem::fileExists(fullPath)) 
    {
        return HttpUtils::ErrorPage(404, "Not Found", config);
    }
    if (!FileSystem::deleteFile(fullPath)) 
    {
        return HttpUtils::ErrorPage(403, "Forbidden", config);
    }
    return HttpResponse(204, "No Content");
}

// static void dbg_print(std::string identifier, std::string data)
// {
//     std::cout << identifier << data << std::endl;
// }

std::string getInBetween(std::string str, std::string s1, std::string s2)
{
    std::string result = str.substr(str.find(s1) + s1.size());
    result = result.substr(0, result.find(s2));
    return result;
}

void storeFile(std::string body, std::string rootDirectory)
{
    std::string disp = getInBetween(body, "Content-Disposition: ", "\r\n");

    std::cout << "trimedBody: " << body;
    // dbg_print("trimedBody: ", body);
    if (disp.find("filename=") != std::string::npos)
    {
        std::string filename = getInBetween(disp, "filename=\"", "\"");
        std::string content = body.substr(body.find("\r\n\r\n") + 4);
        FileSystem::writeToFile(rootDirectory + "/" + filename, content);
    }
}

HttpResponse HttpMethods::POST(const HttpRequest& request, const std::string &rootDirectory, std::string requestPath, const ServerConfig &config)
{
    (void) request;
    (void) rootDirectory;
    (void) requestPath;
    (void) config;
    
    std::string boundary = getInBetween(request.getHeader("content-type"), "boundary=", "\n");
    // dbg_print("boundary is: ", boundary);
    
    std::string trimedBody = getInBetween(request.getBody(), boundary, "--" + boundary);
    storeFile(trimedBody, rootDirectory);
    return HttpResponse(200, "OK");
}

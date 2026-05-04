#include "Methods.hpp"
#include "HttpUtils.hpp"
#include "HttpHandler.hpp"

/*
--BOUNDARY\r\n
Content-Disposition: form-data; name="file"; filename="cat.png"\r\n
Content-Type: image/png\r\n
\r\n
FILE_CONTENT_HERE\r\n
--BOUNDARY--\r\n
*/

static bool isSafeFilename(const std::string &filename)
{
    if (filename.empty())
        return false;

    if (filename.find("..") != std::string::npos)
        return false;

    if (filename.find('/') != std::string::npos)
        return false;

    if (filename.find('\\') != std::string::npos)
        return false;

    return true;
}

static bool extractBoundary(const std::string &contentType, std::string &boundary)
{
    std::string key = "boundary=";
    size_t pos = contentType.find(key);

    if (pos == std::string::npos)
        return false;

    boundary = contentType.substr(pos + key.size());

    size_t semicolon = boundary.find(';');
    if (semicolon != std::string::npos)
        boundary = boundary.substr(0, semicolon);

    boundary = trim(boundary);

    if (boundary.size() >= 2 && boundary[0] == '"' && boundary[boundary.size() - 1] == '"')
        boundary = boundary.substr(1, boundary.size() - 2);

    return !boundary.empty();
}

static bool extractFilename(const std::string &partHeaders, std::string &filename)
{
    std::string key = "filename=\"";
    size_t pos = partHeaders.find(key);

    if (pos == std::string::npos)
        return false;

    pos += key.size();

    size_t end = partHeaders.find("\"", pos);
    if (end == std::string::npos)
        return false;

    filename = partHeaders.substr(pos, end - pos);
    return isSafeFilename(filename);
}

static bool parseMultipartFile(const std::string &body, const std::string &boundary, std::string &filename, std::string &fileContent)
{
    std::string delimiter = "--" + boundary;
    std::string closingDelimiter = "--" + boundary + "--";

    size_t partStart = body.find(delimiter);
    if (partStart == std::string::npos)
        return false;

    partStart += delimiter.size();

    if (body.compare(partStart, 2, "\r\n") != 0)
        return false;

    partStart += 2;

    size_t headersEnd = body.find("\r\n\r\n", partStart);
    if (headersEnd == std::string::npos)
        return false;

    std::string partHeaders = body.substr(partStart, headersEnd - partStart);

    if (!extractFilename(partHeaders, filename))
        return false;

    size_t contentStart = headersEnd + 4;

    size_t nextBoundary = body.find("\r\n" + delimiter, contentStart);
    if (nextBoundary == std::string::npos)
        return false;

    fileContent = body.substr(contentStart, nextBoundary - contentStart);

    return true;
}

HttpResponse HttpMethods::POST(const HttpRequest &request, RouteMatch *match, const ServerConfig &config)
{
    if (!match || !match->location)
        return HttpUtils::ErrorPage(500, "Internal Server Error!", config);

    if (match->location->uploadEnabled != "on")
        return HttpUtils::ErrorPage(403, "Forbidden", config);

    if (match->location->uploadPath.empty())
        return HttpUtils::ErrorPage(500, "Internal Server Error2", config);

    if (!HttpUtils::isDirectory(match->location->uploadPath))
        return HttpUtils::ErrorPage(500, match->location->uploadPath, config);

    std::string contentType = request.getHeader("content-type");

    if (contentType.find("multipart/form-data") == std::string::npos)
        return HttpUtils::ErrorPage(415, "Unsupported Media Type", config);

    std::string boundary;
    if (!extractBoundary(contentType, boundary))
        return HttpUtils::ErrorPage(400, "Bad Request", config);

    std::string filename;
    std::string fileContent;

    if (!parseMultipartFile(request.getBody(), boundary, filename, fileContent))
        return HttpUtils::ErrorPage(400, "Bad Request", config);

    std::string outputPath = joinPath(match->location->uploadPath, filename);

    if (!FileSystem::writeToFile(outputPath, fileContent))
        return HttpUtils::ErrorPage(500, "Internal Server Error4", config);

    HttpResponse response(201, "Created");
    response.setBody("File uploaded successfully\n");
    response.setHeader("Content-Type", "text/plain");
    return response;
}
#include "Methods.hpp"

/*
--BOUNDARY\r\n
Content-Disposition: form-data; name="file"; filename="cat.png"\r\n
Content-Type: image/png\r\n
\r\n
FILE_CONTENT_HERE\r\n
--BOUNDARY--\r\n
*/

struct MultipartFileInfo
{
    std::string filename;
    size_t contentStart;
    size_t contentLength;

    MultipartFileInfo() : contentStart(0), contentLength(0) {}
};

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

static bool parseMultipartFileInfo(const std::string &body, const std::string &boundary, MultipartFileInfo &info)
{
    std::string delimiter = "--" + boundary;

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

    if (!extractFilename(partHeaders, info.filename))
        return false;

    info.contentStart = headersEnd + 4;

    size_t nextBoundary = body.find("\r\n" + delimiter, info.contentStart);
    if (nextBoundary == std::string::npos)
        return false;

    info.contentLength = nextBoundary - info.contentStart;
    return true;
}

static bool writeBufferToFile(const std::string &filePath, const char *data, size_t size)
{
    std::ofstream outfile(filePath.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);

    if (!outfile.is_open())
        return false;

    outfile.write(data, size);
    if (!outfile.good())
        return false;

    outfile.close();
    return true;
}

HttpResponse HttpMethods::POST(const HttpRequest &request, const RouteMatch &match, const ServerConfig &config)
{
    if (!match.location)
        return ErrorPage(500, config);

    if (!match.location || match.location->uploadEnabled != "on")
        return ErrorPage(403, config);

    if (match.location->uploadPath.empty() || !isDirectory(match.location->uploadPath))
        return ErrorPage(500, config);

    std::string contentType = request.getHeader("content-type")[0];
    std::string outputPath;
    const char* dataStart = NULL;
    size_t dataLength = 0;

    if (contentType.find("multipart/form-data") != std::string::npos)
    {
        std::string boundary;
        if (!extractBoundary(contentType, boundary))
            return ErrorPage(400, config);

        MultipartFileInfo fileInfo;
        if (!parseMultipartFileInfo(request.getBody(), boundary, fileInfo))
            return ErrorPage(400, config);

        outputPath = joinPath(match.location->uploadPath, fileInfo.filename);
        dataStart = request.getBody().data() + fileInfo.contentStart;
        dataLength = fileInfo.contentLength;
    }
    else 
    {
        outputPath = joinPath(match.location->uploadPath, "upload.bin");
        dataStart = request.getBody().data();
        dataLength = request.getBody().size();
    }

    if (!dataStart || (dataStart + dataLength > request.getBody().data() + request.getBody().size()))
        return ErrorPage(400, config);

    if (!writeBufferToFile(outputPath, dataStart, dataLength))
        return ErrorPage(500, config);

    HttpResponse response(201, "Created");
    response.setBody("File uploaded successfully to: " + outputPath + "\n");
    response.setHeader("Content-Type", "text/plain");

    return response;
}
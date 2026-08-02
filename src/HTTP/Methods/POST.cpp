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

    if (match.location->uploadEnabled != "on")
        return ErrorPage(405, config);

    if (match.location->uploadPath.empty())
        return ErrorPage(500, config);

    std::string uploadDirectory = joinPath(match.root, match.location->uploadPath);

    if (!isDirectory(uploadDirectory))
        return ErrorPage(500, config);

    const std::string &body = request.getBody();

    std::string outputPath;
    const char *dataStart = body.data();
    size_t dataLength = body.size();

    if (request.hasContentType() && request.getContentType().mediaType == "multipart/form-data")
    {
        const ContentTypeData &contentType = request.getContentType();
        std::map<std::string, std::string>::const_iterator boundaryIt;

        boundaryIt = contentType.parameters.find("boundary");

        if (boundaryIt == contentType.parameters.end() || boundaryIt->second.empty())
            return ErrorPage(400, config);

        MultipartFileInfo fileInfo;

        if (!parseMultipartFileInfo(body, boundaryIt->second, fileInfo))
            return ErrorPage(400, config);

        if (fileInfo.filename.empty())
            return ErrorPage(400, config);

        if (fileInfo.filename.find('/') != std::string::npos || fileInfo.filename.find('\\') != std::string::npos || fileInfo.filename == "." || fileInfo.filename == "..")
        {
            return ErrorPage(400, config);
        }
        if (fileInfo.contentStart > body.size() || fileInfo.contentLength > body.size() - fileInfo.contentStart)
        {
            return ErrorPage(400, config);
        }

        outputPath = joinPath(uploadDirectory, fileInfo.filename);
        dataStart = body.data() + fileInfo.contentStart;
        dataLength = fileInfo.contentLength;
    }
    else
    {
        outputPath = joinPath(uploadDirectory, "upload.bin");
    }

    if (!writeBufferToFile(outputPath, dataStart, dataLength))
        return ErrorPage(500, config);

    HttpResponse response(201, "Created");

    response.setBody("File uploaded successfully to: " + outputPath + "\n");
    response.setHeader("Content-Type", "text/plain");

    return response;
}
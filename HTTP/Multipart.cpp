#include "HttpHandler.hpp"
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

bool extractBoundary(const std::string &contentType, std::string &boundary)
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

bool parseMultipartFile(const std::string &body, const std::string &boundary, std::string &filename, std::string &fileContent)
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


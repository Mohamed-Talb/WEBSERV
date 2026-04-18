// HttpRequest.cpp
#include "HttpRequest.hpp"
#include <cctype>

static std::string toUpper(std::string value)
{
    for (size_t i = 0; i < value.size(); ++i)
        value[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(value[i])));
    return value;
}

static std::string toLower(std::string value)
{
    for (size_t i = 0; i < value.size(); ++i)
        value[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(value[i])));
    return value;
}

static std::string trim(const std::string& value)
{
    size_t start = 0;
    while (start < value.size() && (value[start] == ' ' || value[start] == '\t'))
        ++start;
    size_t end = value.size();
    while (end > start && (value[end - 1] == ' ' || value[end - 1] == '\t'))
        --end;
    return value.substr(start, end - start);
}

static bool parseSize(const std::string& token, size_t& value)
{
    if (token.empty())
        return false;
    std::istringstream iss(token);
    size_t parsed = 0;
    iss >> parsed;
    if (iss.fail() || !iss.eof())
        return false;
    value = parsed;
    return true;
}

static bool parseChunkedBody(const std::string& rawBody, std::string& outBody, size_t& consumed)
{
    outBody.clear();
    consumed = 0;
    size_t cursor = 0;
    while (true)
    {
        size_t lineEnd = rawBody.find("\r\n", cursor);
        if (lineEnd == std::string::npos)
            return false;
        std::string sizeLine = rawBody.substr(cursor, lineEnd - cursor);
        size_t semi = sizeLine.find(';');
        if (semi != std::string::npos)
            sizeLine = sizeLine.substr(0, semi);
        std::istringstream iss(sizeLine);
        size_t chunkSize = 0;
        iss >> std::hex >> chunkSize;
        if (iss.fail() || !iss.eof())
            return false;
        cursor = lineEnd + 2;
        if (rawBody.size() < cursor + chunkSize + 2)
            return false;
        if (chunkSize == 0)
        {
            if (rawBody.size() < cursor + 2)
                return false;
            if (rawBody.substr(cursor, 2) != "\r\n")
                return false;
            consumed = cursor + 2;
            return true;
        }
        outBody.append(rawBody, cursor, chunkSize);
        cursor += chunkSize;
        if (rawBody.substr(cursor, 2) != "\r\n")
            return false;
        cursor += 2;
    }
}

HttpRequest::HttpRequest() : consumedBytes(0), errorCode(0) {}
HttpRequest::~HttpRequest() {}

int HttpRequest::parse(const std::string& rawBuffer)
{
    size_t headerEnd = rawBuffer.find("\r\n\r\n");
    if (headerEnd == std::string::npos)
        return 0;

    std::string headerBlock = rawBuffer.substr(0, headerEnd);
    std::istringstream headerStream(headerBlock);
    std::string requestLine;

    if (!std::getline(headerStream, requestLine))
    {
        errorCode = 400;
        return 1;
    }

    if (!requestLine.empty() && requestLine[requestLine.size() - 1] == '\r')
        requestLine.erase(requestLine.size() - 1);
    std::istringstream rl(requestLine);
    if (!(rl >> method >> target >> version))
    {
        errorCode = 400;
        return 1;
    }
    method = toUpper(method);

    std::string headerLine;
    while (std::getline(headerStream, headerLine))
    {
        if (!headerLine.empty() && headerLine[headerLine.size() - 1] == '\r')
            headerLine.erase(headerLine.size() - 1);
        if (headerLine.empty())
            continue;
        size_t sep = headerLine.find(':');
        if (sep == std::string::npos)
        {
            errorCode = 400;
            return 1;
        }
        std::string name = toLower(trim(headerLine.substr(0, sep)));
        std::string value = trim(headerLine.substr(sep + 1));
        headers[name] = value;
    }

    if (version != "HTTP/1.1" && version != "HTTP/1.0")
    {
        errorCode = 505;
        return 1;
    }
    if (version == "HTTP/1.1" && headers.find("host") == headers.end())
    {
        errorCode = 400;
        return 1;
    }

    size_t consumedBody = 0;
    std::string payload = rawBuffer.substr(headerEnd + 4);

    if (headers.count("transfer-encoding") && headers.count("content-length"))
    {
        errorCode = 400;
        return 1;
    }

    if (headers.count("transfer-encoding"))
    {
        if (toLower(headers["transfer-encoding"]) != "chunked")
        {
            errorCode = 400;
            return 1;
        }
        if (!parseChunkedBody(payload, body, consumedBody))
            return 0;
    }
    else if (headers.count("content-length"))
    {
        size_t contentLength = 0;
        if (!parseSize(headers.at("content-length"), contentLength))
        {
            errorCode = 400;
            return 1;
        }
        if (payload.size() < contentLength)
            return 0;
        body = payload.substr(0, contentLength);
        consumedBody = contentLength;
    }

    consumedBytes = headerEnd + 4 + consumedBody;
    return 1;
}

std::string HttpRequest::getMethod() const { return method; }
std::string HttpRequest::getTarget() const { return target; }
std::string HttpRequest::getVersion() const { return version; }
std::string HttpRequest::getBody() const { return body; }
size_t HttpRequest::getConsumedBytes() const { return consumedBytes; }
int HttpRequest::getErrorCode() const { return errorCode; }

std::string HttpRequest::getHeader(const std::string& key) const
{
    std::map<std::string, std::string>::const_iterator it = headers.find(key);
    if (it != headers.end())
        return it->second;
    return "";
}
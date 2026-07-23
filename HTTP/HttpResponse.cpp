#include "HttpResponse.hpp"

HttpResponse::HttpResponse() : statusCode(200), reasonPhrase("OK")
{
    headers["Connection"] = "keep-alive";
}

HttpResponse::HttpResponse(int code, const std::string &reason) : statusCode(code), reasonPhrase(reason)
{
    headers["Connection"] = "keep-alive";
}

HttpResponse::~HttpResponse() {}

void HttpResponse::setHeader(const std::string &name, const std::string &value)
{
    headers[toLower(name)] = value;
}

void HttpResponse::setBody(const std::string &content)
{
    body = content;

    std::ostringstream sizeStream;
    sizeStream << body.size();
    headers["Content-Length"] = sizeStream.str();
}

void HttpResponse::writeBody(const std::string &chunk)
{
    body += chunk;

    std::ostringstream sizeStream;
    sizeStream << body.size();
    headers["Content-Length"] = sizeStream.str();
}

bool HttpResponse::setBodyFromFile(const std::string &filePath)
{
    std::ifstream file(filePath.c_str(), std::ios::in | std::ios::binary);

    if (!file.is_open())
        return false;

    std::ostringstream buffer;
    buffer << file.rdbuf();

    if (file.bad())
        return false;

    body = buffer.str();

    std::ostringstream sizeStream;
    sizeStream << body.size();
    headers["Content-Length"] = sizeStream.str();

    return true;
}

std::string HttpResponse::toString() const
{
    std::ostringstream responseStream;
    responseStream << "HTTP/1.1 " << statusCode << " " << reasonPhrase << "\r\n";
    std::cout << "[RESPONSE]: HTTP/1.1 " << statusCode << " " << reasonPhrase << std::endl;
    std::map<std::string, std::string>::const_iterator headerIterator;
    for (headerIterator = headers.begin(); headerIterator != headers.end(); ++headerIterator)
    {
        responseStream << headerIterator->first << ": " << headerIterator->second << "\r\n";
    }
    responseStream << "\r\n" << body;
    return responseStream.str();
}
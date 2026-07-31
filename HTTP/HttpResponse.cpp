#include "HttpResponse.hpp"

HttpResponse::HttpResponse() : statusCode(200), reasonPhrase("OK")
{
    setHeader("Connection", "keep-alive");
}

HttpResponse::HttpResponse(int code, const std::string &reason)
    : statusCode(code), reasonPhrase(reason)
{
    setHeader("Connection", "keep-alive");
}

HttpResponse::~HttpResponse()
{
}

void HttpResponse::setHeader(const std::string &key, const std::string &value)
{
    headers[key].clear();
    headers[key].push_back(value);
}

void HttpResponse::addHeader(const std::string &key, const std::string &value)
{
    headers[key].push_back(value);
}

void HttpResponse::setBody(const std::string &content)
{
    body = content;

    std::ostringstream sizeStream;
    sizeStream << body.size();

    setHeader("Content-Length", sizeStream.str());
}

void HttpResponse::writeBody(const std::string &chunk)
{
    body += chunk;

    std::ostringstream sizeStream;
    sizeStream << body.size();

    setHeader("Content-Length", sizeStream.str());
}

std::string HttpResponse::toString() const
{
    std::ostringstream responseStream;

    responseStream << "HTTP/1.1 " << statusCode << " "
                   << reasonPhrase << "\r\n";

    std::map<std::string, std::vector<std::string> >::const_iterator headerIterator;

    for (headerIterator = headers.begin(); headerIterator != headers.end(); ++headerIterator)
    {
        const std::vector<std::string> &values = headerIterator->second;

        for (size_t i = 0; i < values.size(); ++i)
        {
            responseStream << headerIterator->first << ": "
                           << values[i] << "\r\n";
        }
    }

    responseStream << "\r\n";
    responseStream << body;

    return responseStream.str();
}

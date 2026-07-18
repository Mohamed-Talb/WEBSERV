#include "HttpRequest.hpp"
#include "../Helpers.hpp"

HttpRequest::HttpRequest()
{
}

void HttpRequest::reset()
{
    method.clear();
    target.clear();
    version.clear();
    requestPath.clear();
    query.clear();
    body.clear();
    headers.clear();
}

void HttpRequest::setMethod(const std::string &value)
{
    method = value;
}

void HttpRequest::setTarget(const std::string &value)
{
    target = value;
}

void HttpRequest::setVersion(const std::string &value)
{
    version = value;
}

void HttpRequest::setRequestPath(const std::string &value)
{
    requestPath = value;
}

void HttpRequest::setQuery(const std::string &value)
{
    query = value;
}

void HttpRequest::setHeader(const std::string &key, const std::string &value)
{
    headers[toLower(key)] = value;
}

void HttpRequest::appendHeader(const std::string &key, const std::string &value)
{
    std::string normalizedKey = toLower(key);
    std::map<std::string, std::string>::iterator it = headers.find(normalizedKey);

    if (it == headers.end())
        headers[normalizedKey] = value;
    else
        it->second += ", " + value;
}

void HttpRequest::appendBody(const std::string &value)
{
    body += value;
}

bool HttpRequest::hasHeader(const std::string &key) const
{
    return headers.find(toLower(key)) != headers.end();
}

bool HttpRequest::shouldCloseConnection() const
{
    std::string connection = toLower(getHeader("connection"));

    if (version == "HTTP/1.0")
        return connection != "keep-alive";

    if (version == "HTTP/1.1")
        return connection == "close";

    return true;
}

const std::string &HttpRequest::getMethod() const
{
    return method;
}

const std::string &HttpRequest::getVersion() const
{
    return version;
}

const std::string &HttpRequest::getRequestPath() const
{
    return requestPath;
}

const std::string &HttpRequest::getQuery() const
{
    return query;
}

const std::string &HttpRequest::getBody() const
{
    return body;
}

const std::string &HttpRequest::getHeader(const std::string &key) const
{
    std::map<std::string, std::string>::const_iterator it =
        headers.find(toLower(key));

    if (it != headers.end())
        return it->second;

    static const std::string empty;
    return empty;
}

const std::map<std::string, std::string> &HttpRequest::getHeaders() const
{
    return headers;
}

const std::string &HttpRequest::getTarget() const
{
    return target;
}


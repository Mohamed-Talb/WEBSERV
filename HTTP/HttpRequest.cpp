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

const std::string &HttpRequest::getBody() const { return body; }
const std::string &HttpRequest::getQuery() const { return query; }
const std::string &HttpRequest::getMethod() const { return method; }
const std::string &HttpRequest::getTarget() const { return target; }
const std::string &HttpRequest::getVersion() const { return version; }
const std::string &HttpRequest::getRequestPath() const { return requestPath; }

void HttpRequest::setQuery(const std::string &value) { query = value; }
void HttpRequest::setTarget(const std::string &value) { target = value; }
void HttpRequest::setMethod(const std::string &value) { method = value; }
void HttpRequest::appendBody(const char *data, size_t size)
{
    body.append(data, size);
}

void HttpRequest::reserveBody(size_t size)
{
    body.reserve(size);
}
void HttpRequest::setVersion(const std::string &value) { version = value; }
void HttpRequest::setRequestPath(const std::string &value) { requestPath = value; }

const std::map<std::string, std::vector<std::string> > &HttpRequest::getHeaders() const { return headers; }

void HttpRequest::setHeader(const std::string &key, const std::string &value)
{
    std::string normalizedKey = toLower(key);
    headers[normalizedKey].clear();
    headers[normalizedKey].push_back(value);
}

void HttpRequest::appendHeader(const std::string &key, const std::string &value)
{
    headers[toLower(key)].push_back(value);
}

bool HttpRequest::hasHeader(const std::string &key) const
{
    return headers.find(toLower(key)) != headers.end();
}

const std::vector<std::string> &HttpRequest::getHeaderValues(const std::string &key) const
{
    std::map<std::string, std::vector<std::string> >::const_iterator it = headers.find(toLower(key));

    if (it != headers.end())
        return it->second;

    static const std::vector<std::string> empty;
    return empty;
}

const std::vector<std::string> &HttpRequest::getHeader(const std::string &key) const
{
    std::map<std::string, std::vector<std::string> >::const_iterator it = headers.find(toLower(key));

    if (it != headers.end())
        return it->second;

    static const std::vector<std::string> empty;
    return empty;
}

bool HttpRequest::headerContainsToken(const std::string &key, const std::string &expected) const
{
    const std::vector<std::string> &values = getHeaderValues(key);
    std::string expectedLower = toLower(expected);

    for (size_t i = 0; i < values.size(); ++i)
    {
        if (toLower(trim(values[i])) == expectedLower)
            return true;
    }

    return false;
}

bool HttpRequest::shouldCloseConnection() const
{
    if (version == "HTTP/1.0")
        return !headerContainsToken("connection", "keep-alive");

    if (version == "HTTP/1.1")
        return headerContainsToken("connection", "close");

    return true;
}
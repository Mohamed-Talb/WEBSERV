#include "HttpRequest.hpp"

ContentTypeData::ContentTypeData()
{
}

void ContentTypeData::reset()
{
    raw.clear();
    mediaType.clear();
    parameters.clear();
}

HttpRequest::HttpRequest(): hostPresent(false), contentLengthPresent(false), contentLength(0), contentTypePresent(false), chunked(false), closeConnection(false)
{}

void HttpRequest::reset()
{
    method.clear();
    target.clear();
    version.clear();
    requestPath.clear();
    query.clear();
    body.clear();

    rawHeaders.clear();

    hostPresent = false;
    host.clear();

    contentLengthPresent = false;
    contentLength = 0;

    contentTypePresent = false;
    contentType.reset();

    chunked = false;
    closeConnection = false;
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

void HttpRequest::appendHeader(const std::string &name, const std::string &value)
{
    rawHeaders[toLower(name)].push_back(value);
}

void HttpRequest::setHost(const std::string &value)
{
    host = value;
    hostPresent = true;
}

void HttpRequest::setContentLength(size_t value)
{
    contentLength = value;
    contentLengthPresent = true;
}

void HttpRequest::setContentType(const ContentTypeData &value)
{
    contentType = value;
    contentTypePresent = true;
}

void HttpRequest::setChunked(bool value)
{
    chunked = value;
}

void HttpRequest::setCloseConnection(bool value)
{
    closeConnection = value;
}

void HttpRequest::appendToBody(const char *data, size_t size)
{
    if (!data || size == 0)
        return;

    body.append(data, size);
}

void HttpRequest::reserveBody(size_t size)
{
    body.reserve(size);
}

bool HttpRequest::hasHeader(const std::string &name) const
{
    return rawHeaders.find(toLower(name)) != rawHeaders.end();
}

bool HttpRequest::hasHost() const
{
    return hostPresent;
}

bool HttpRequest::hasContentLength() const
{
    return contentLengthPresent;
}

bool HttpRequest::hasContentType() const
{
    return contentTypePresent;
}

bool HttpRequest::isChunked() const
{
    return chunked;
}

bool HttpRequest::shouldCloseConnection() const
{
    return closeConnection;
}

const std::string &HttpRequest::getMethod() const
{
    return method;
}

const std::string &HttpRequest::getTarget() const
{
    return target;
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

const std::string &HttpRequest::getHost() const
{
    return host;
}

size_t HttpRequest::getContentLength() const
{
    return contentLength;
}

const ContentTypeData &HttpRequest::getContentType() const
{
    return contentType;
}

const std::vector<std::string> &HttpRequest::getRawHeader(const std::string &name) const
{
    static const std::vector<std::string> empty;
    std::map<std::string, std::vector<std::string> >::const_iterator it;

    it = rawHeaders.find(toLower(name));

    if (it == rawHeaders.end())
        return empty;

    return it->second;
}

const std::map<std::string, std::vector<std::string> > &HttpRequest::getRawHeaders() const
{
    return rawHeaders;
}


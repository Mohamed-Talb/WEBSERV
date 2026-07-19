#ifndef HTTP_REQUEST_HPP
#define HTTP_REQUEST_HPP

#include <map>
#include <string>

class HttpRequest
{
private:
    std::string method;
    std::string target;
    std::string version;
    std::string requestPath;
    std::string query;
    std::string body;

    std::map<std::string, std::string> headers;

public:
    HttpRequest();

    void reset();

    void setMethod(const std::string &value);
    void setTarget(const std::string &value);
    void setVersion(const std::string &value);
    void setRequestPath(const std::string &value);
    void setQuery(const std::string &value);

    void setHeader(const std::string &key, const std::string &value);
    void appendHeader(const std::string &key, const std::string &value);
    void appendBody(const std::string &value);

    bool hasHeader(const std::string &key) const;
    bool shouldCloseConnection() const;

    const std::string &getMethod() const;
    const std::string &getVersion() const;
    const std::string &getRequestPath() const;
    const std::string &getQuery() const;
    const std::string &getBody() const;
    const std::string &getTarget() const;
    const std::string &getHeader(const std::string &key) const;

    const std::map<std::string, std::string> &getHeaders() const;
};

#endif
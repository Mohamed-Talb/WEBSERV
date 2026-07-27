#ifndef HTTP_REQUEST_HPP
#define HTTP_REQUEST_HPP

#include <map>
#include <string>
#include <vector>

class HttpRequest
{
    private:
    std::string method;
    std::string target;
    std::string version;
    std::string requestPath;
    std::string query;
    std::string body;

    std::map<std::string, std::vector<std::string> > headers;
    std::map<std::string, std::string> cookies;
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
    void appendBody(const char *data, size_t size);
    void reserveBody(size_t size);

    bool hasHeader(const std::string &key) const;
    bool headerContainsToken(const std::string &key, const std::string &expected) const;
    bool shouldCloseConnection() const;

    const std::string &getMethod() const;
    const std::string &getVersion() const;
    const std::string &getRequestPath() const;
    const std::string &getQuery() const;
    const std::string &getBody() const;
    const std::string &getTarget() const;

    bool hasCookie(const std::string &name) const;
    void setCookie(const std::string &name, const std::string &value);
    bool getCookie(const std::string &name, std::string &value) const;

    const std::map<std::string, std::string> &getCookies() const;
    
    const std::vector<std::string> &getHeader(const std::string &key) const;
    const std::vector<std::string> &getHeaderValues(const std::string &key) const;
    const std::map<std::string, std::vector<std::string> > &getHeaders() const;
};

#endif
#ifndef HTTP_REQUEST_HPP
#define HTTP_REQUEST_HPP

#include <cstddef>
#include <map>
#include <string>
#include <vector>
#include "../../Helpers.hpp"

struct ContentTypeData
{
    std::string raw;
    std::string mediaType;
    std::map<std::string, std::string> parameters;

    ContentTypeData();
    void reset();
};

class HttpRequest
{
    private:
        std::string method;
        std::string target;
        std::string version;
        std::string requestPath;
        std::string query;
        std::string body;

        std::map<std::string, std::vector<std::string> > rawHeaders;

        bool hostPresent;
        std::string host;

        bool contentLengthPresent;
        size_t contentLength;

        bool contentTypePresent;
        ContentTypeData contentType;

        bool chunked;
        bool closeConnection;

    public:
        HttpRequest();

        void reset();

        void setMethod(const std::string &value);
        void setTarget(const std::string &value);
        void setVersion(const std::string &value);
        void setRequestPath(const std::string &value);
        void setQuery(const std::string &value);

        void appendHeader(const std::string &name, const std::string &value);
        void setHost(const std::string &value);
        void setContentLength(size_t value);
        void setContentType(const ContentTypeData &value);
        void setChunked(bool value);
        void setCloseConnection(bool value);

        void appendToBody(const char *data, size_t size);
        void reserveBody(size_t size);

        bool shouldCloseConnection() const;
        bool hasHost() const;
        bool isChunked() const;
        bool hasContentType() const;
        bool hasContentLength() const;
        bool hasHeader(const std::string &name) const;

        const std::string &getBody() const;
        const std::string &getQuery() const;
        const std::string &getMethod() const;
        const std::string &getTarget() const;
        const std::string &getVersion() const;
        const std::string &getRequestPath() const;

        const std::string &getHost() const;
        size_t getContentLength() const;
        const ContentTypeData &getContentType() const;

        const std::vector<std::string> &getRawHeader(const std::string &name) const;
        const std::map<std::string, std::vector<std::string> > &getRawHeaders() const;

};

#endif
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
        std::string body;
        std::string query;
        std::string target;
        std::string method;
        std::string version;
        std::string requestPath;
        std::map<std::string, std::vector<std::string> > rawHeaders;

        bool chunked;
        bool hostPresent;
        std::string host;
        size_t contentLength;
        bool closeConnection;
        bool contentTypePresent;
        bool contentLengthPresent;
        ContentTypeData contentType;


        public:
        HttpRequest();
        void reset();

        
        void setChunked(bool value);
        void setCloseConnection(bool value);
        void setContentLength(size_t value);
        void setHost(const std::string &value);
        void setQuery(const std::string &value);
        void setMethod(const std::string &value);
        void setTarget(const std::string &value);
        void setVersion(const std::string &value);
        void setRequestPath(const std::string &value);
        void setContentType(const ContentTypeData &value);
        void appendHeader(const std::string &name, const std::string &value);
        
        void reserveBody(size_t size);
        void appendToBody(const char *data, size_t size);

        bool hasHost() const;
        bool isChunked() const;
        bool hasContentType() const;
        bool hasContentLength() const;
        bool shouldCloseConnection() const;
        bool hasHeader(const std::string &name) const;

        const std::string &getBody() const;
        const std::string &getQuery() const;
        const std::string &getMethod() const;
        const std::string &getTarget() const;
        const std::string &getVersion() const;
        const std::string &getRequestPath() const;

        size_t getContentLength() const;
        const std::string &getHost() const;
        const ContentTypeData &getContentType() const;

        const std::vector<std::string> &getRawHeader(const std::string &name) const;
        const std::map<std::string, std::vector<std::string> > &getRawHeaders() const;

};

#endif
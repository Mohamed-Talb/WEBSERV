#ifndef HTTPREQUEST_HPP
#define HTTPREQUEST_HPP

#include <map>
#include <string>
#include <cstddef>

enum ParseStatus
{
    PARSE_NEED_MORE_DATA,
    PARSE_HEADERS_COMPLETE,
    PARSE_REQUEST_COMPLETE
};

enum State
{
    PARSE_REQUEST_LINE,
    PARSE_HEADERS,
    PARSE_BODY,
    PARSE_COMPLETE,
    PARSE_ERROR
};

enum StepStatus
{
    STEP_ERROR = -1,
    STEP_NEED_MORE_DATA = 0,
    STEP_COMPLETE = 1
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

    std::map<std::string, std::string> headers;

    size_t maxBodySize;
    size_t parsedSize;

    State state;
    int errorCode;

    private:
    void setError(int code);

    bool splitTarget();

    StepStatus parseRequestLine(const std::string &raw);
    StepStatus parseHeaders(const std::string &raw);
    StepStatus parseBody(const std::string &raw);
    StepStatus parseChunkedBody(const std::string &raw);

    public:
    HttpRequest();
    ~HttpRequest();

    ParseStatus parse(const std::string &rawRequestData);
    void reset();

    void setMaxBodySize(size_t value);

    const std::string &getHeader(const std::string &key) const;
    const std::string &getBody() const;
    const std::string &getMethod() const;
    const std::string &getTarget() const;
    const std::string &getVersion() const;
    const std::string &getQuery() const;
    const std::string &getRequestPath() const;

    int getErrorCode() const;
    size_t getParsedSize() const;

    bool shouldCloseConnection() const;
};

#endif
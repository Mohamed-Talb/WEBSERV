#ifndef HTTP_REQUEST_PARSER_HPP
#define HTTP_REQUEST_PARSER_HPP


#include <cstddef>
#include <string>
#include "HttpRequest.hpp"

enum ParseStatus
{
    PARSE_NEED_MORE_DATA,
    PARSE_HEADERS_COMPLETE,
    PARSE_REQUEST_COMPLETE,
    PARSE_REQUEST_ERROR
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

class HttpRequestParser
{
    private:
    HttpRequest request;

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
    HttpRequestParser();
    ~HttpRequestParser();

    ParseStatus parse(const std::string &rawRequestData);

    void reset();

    void setMaxBodySize(size_t value);

    size_t getParsedSize() const;
    int getErrorCode() const;

    HttpRequest &getRequest();
    const HttpRequest &getRequest() const;
};

#endif
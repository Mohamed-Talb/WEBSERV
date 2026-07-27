#ifndef HTTP_REQUEST_PARSER_HPP
#define HTTP_REQUEST_PARSER_HPP


#include <cstddef>
#include <string>
#include "HttpRequest.hpp"
#include "../configParser/configParser.hpp"

const size_t MAX_HEADER_SIZE = 16384;
const size_t MAX_REQUEST_LINE_SIZE = 8192;

enum ParseStatus
{
    PARSE_NEED_MORE_DATA,
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

    const std::vector<ServerConfig *> configs;
    const ServerConfig *activeConfig;
    private:
    void setError(int code);

    bool splitTarget();

    StepStatus parseBody(const std::string &raw);
    StepStatus parseHeaders(const std::string &raw);
    StepStatus parseRequestLine(const std::string &raw);
    StepStatus parseChunkedBody(const std::string &raw);
    bool       storeHeader(const std::string &key, const std::string &value);
    
    public:
    HttpRequestParser(const std::vector<ServerConfig *> &conf);
    ~HttpRequestParser();
    const ServerConfig *getActiveConfig();

    ParseStatus parse(const std::string &rawRequestData);

    void reset();

    void setMaxBodySize(size_t value);

    size_t getParsedSize() const;
    int getErrorCode() const;

    HttpRequest &getRequest();
    const HttpRequest &getRequest() const;
};

#endif
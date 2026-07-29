#ifndef REQUEST_PARSER_HPP
#define REQUEST_PARSER_HPP


#include <cstddef>
#include <string>
#include "HttpRequest.hpp"
#include "../configParser/configParser.hpp"
#include "../../Helpers.hpp"
#include "../HttpUtils/HttpUtils.hpp"
#include <algorithm>

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

class RequestParser
{
    private:
    HttpRequest request;

    size_t maxBodySize;
    size_t parsedSize;
    size_t expectedBodySize;
    bool bodySizeInitialized;

    State state;
    int errorCode;

    const std::vector<ServerConfig *> configs;
    const ServerConfig *activeConfig;
    private:
    void setError(int code);

    StepStatus bodyParser(const std::string &raw);
    StepStatus headersParser(const std::string &raw);
    StepStatus parseNormalBody(const std::string &raw);
    StepStatus parseChunkedBody(const std::string &raw);
    StepStatus requestLineParser(const std::string &raw);
    bool initializeRequestConfig();
    
    public:
    RequestParser(const std::vector<ServerConfig *> &conf);
    ~RequestParser();
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
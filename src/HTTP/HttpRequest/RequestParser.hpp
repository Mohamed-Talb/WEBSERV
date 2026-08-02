#ifndef REQUEST_PARSER_HPP
#define REQUEST_PARSER_HPP


#include <cstddef>
#include <string>
#include "HttpRequest.hpp"
#include "../../configParser/configParser.hpp"
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
    
    State state;
    int errorCode;
    size_t parsedSize;
    size_t maxBodySize;
    HttpRequest request;
    size_t expectedBodySize;
    bool bodySizeInitialized;


    const ServerConfig *activeConfig;
    const std::vector<ServerConfig *> configs;
    private:
    
    void setError(int code);
    bool initializeRequestConfig();
    StepStatus bodyParser(const std::string &raw);
    StepStatus headersParser(const std::string &raw);
    StepStatus parseNormalBody(const std::string &raw);
    StepStatus parseChunkedBody(const std::string &raw);
    StepStatus requestLineParser(const std::string &raw);
    
    public:
    void reset();
    ~RequestParser();
    void resetParsedSize();
    void setMaxBodySize(size_t value);
    const ServerConfig *getActiveConfig();
    ParseStatus parse(const std::string &rawRequestData);
    RequestParser(const std::vector<ServerConfig *> &conf);

    int getErrorCode() const;
    HttpRequest &getRequest();
    size_t getParsedSize() const;
    const HttpRequest &getRequest() const;
};

#endif
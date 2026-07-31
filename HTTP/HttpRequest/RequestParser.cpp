#include "RequestParser.hpp"

#include "../HttpUtils/HttpUtils.hpp"

RequestParser::RequestParser(const std::vector<ServerConfig *> &conf)
    : maxBodySize(0),
      parsedSize(0),
      expectedBodySize(0),
      bodySizeInitialized(false),
      state(PARSE_REQUEST_LINE),
      errorCode(0),
      configs(conf),
      activeConfig(NULL) {}

RequestParser::~RequestParser() {}

HttpRequest &RequestParser::getRequest()
{
    return request;
}

const HttpRequest &RequestParser::getRequest() const
{
    return request;
}

int RequestParser::getErrorCode() const
{
    return errorCode;
}

size_t RequestParser::getParsedSize() const
{
    return parsedSize;
}

const ServerConfig *RequestParser::getActiveConfig()
{
    return activeConfig;
}

void RequestParser::setError(int code)
{
    errorCode = code;
    state = PARSE_ERROR;
}

void RequestParser::reset()
{
    request.reset();

    maxBodySize = 0;
    parsedSize = 0;
    expectedBodySize = 0;
    bodySizeInitialized = false;
    state = PARSE_REQUEST_LINE;
    errorCode = 0;
    activeConfig = NULL;
}

bool RequestParser::initializeRequestConfig()
{
    if (configs.empty())
    {
        setError(500);
        return false;
    }

    std::string host;

    if (request.hasHost())
        host = request.getHost();

    activeConfig = matchConfig(configs, host);

    if (!activeConfig)
    {
        setError(500);
        return false;
    }

    const Location *location = matchLocation(*activeConfig, request.getRequestPath());

    maxBodySize = activeConfig->client_max_body_size;

    if (location)
        maxBodySize = location->client_max_body_size;

    if (request.hasContentLength()
        && maxBodySize > 0
        && request.getContentLength() > maxBodySize)
    {
        setError(413);
        return false;
    }

    return true;
}

ParseStatus RequestParser::parse(const std::string &rawRequestData)
{
    while (state != PARSE_COMPLETE && state != PARSE_ERROR)
    {
        State previousState = state;
        StepStatus stepStatus = STEP_ERROR;

        switch (state)
        {
            case PARSE_REQUEST_LINE:
                stepStatus = requestLineParser(rawRequestData);
                break;

            case PARSE_HEADERS:
                stepStatus = headersParser(rawRequestData);
                break;

            case PARSE_BODY:
                stepStatus = bodyParser(rawRequestData);
                break;

            default:
                setError(500);
                return PARSE_REQUEST_ERROR;
        }

        if (stepStatus == STEP_NEED_MORE_DATA)
            return PARSE_NEED_MORE_DATA;

        if (stepStatus == STEP_ERROR)
            return PARSE_REQUEST_ERROR;

        if (previousState == PARSE_HEADERS && state == PARSE_BODY)
        {
            if (!initializeRequestConfig())
                return PARSE_REQUEST_ERROR;
        }
    }

    if (state == PARSE_ERROR)
        return PARSE_REQUEST_ERROR;

    if (state == PARSE_COMPLETE)
        return PARSE_REQUEST_COMPLETE;

    return PARSE_NEED_MORE_DATA;
}

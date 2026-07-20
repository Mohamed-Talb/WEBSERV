#include "HttpRequestParser.hpp"

#include "../Helpers.hpp"
#include "./HttpUtils/HttpUtils.hpp"

#include <cctype>
#include <sstream>
#include <algorithm>


HttpRequestParser::HttpRequestParser() : maxBodySize(0), parsedSize(0), state(PARSE_REQUEST_LINE), errorCode(0) {}

HttpRequestParser::~HttpRequestParser() {}


HttpRequest &HttpRequestParser::getRequest() { return request; }
int HttpRequestParser::getErrorCode() const { return errorCode; }
size_t HttpRequestParser::getParsedSize() const { return parsedSize; }
const HttpRequest &HttpRequestParser::getRequest() const { return request; }
void HttpRequestParser::setMaxBodySize(size_t value) { maxBodySize = value; }

void HttpRequestParser::setError(int code)
{
    errorCode = code;
    state = PARSE_ERROR;
}

void HttpRequestParser::reset()
{
    request.reset();

    maxBodySize = 0;
    parsedSize = 0;
    errorCode = 0;
    state = PARSE_REQUEST_LINE;
}

bool HttpRequestParser::splitTarget()
{
    const std::string &target = request.getTarget();

    size_t queryPosition = target.find('?');
    std::string rawPath;
    std::string query;

    if (queryPosition == std::string::npos)
    {
        rawPath = target;
        query.clear();
    }
    else
    {
        rawPath = target.substr(0, queryPosition);
        query = target.substr(queryPosition + 1);
    }
    std::string requestPath;
    if (!urlDecode(rawPath, requestPath))
    {
        setError(400);
        return false;
    }
    bool hadTrailingSlash = requestPath.size() > 1 && requestPath[requestPath.size() - 1] == '/';

    requestPath = normalizePath(requestPath);
    if (hadTrailingSlash && requestPath.size() > 1 && requestPath[requestPath.size() - 1] != '/')
    {
        requestPath += "/";
    }
    if (requestPath.empty())
        requestPath = "/";

    request.setRequestPath(requestPath);
    request.setQuery(query);

    return true;
}

StepStatus HttpRequestParser::parseRequestLine(const std::string &raw)
{
    size_t lineEnd = raw.find("\r\n", parsedSize);
    if (lineEnd == std::string::npos)
    {
        if (raw.size() - parsedSize > MAX_REQUEST_LINE_SIZE)
        {
            setError(414);
            return STEP_ERROR;
        }
        return STEP_NEED_MORE_DATA;
    }
    if (lineEnd - parsedSize > MAX_REQUEST_LINE_SIZE)
    {
        setError(414);
        return STEP_ERROR;
    }
    std::string line = raw.substr(parsedSize, lineEnd - parsedSize);
    std::istringstream lineStream(line);

    std::string method;
    std::string target;
    std::string version;
    std::string extraValue;

    if (!(lineStream >> method >> target >> version)
        || (lineStream >> extraValue))
    {
        setError(400);
        return STEP_ERROR;
    }

    if (target.empty() || target[0] != '/')
    {
        setError(400);
        return STEP_ERROR;
    }

    request.setMethod(method);
    request.setTarget(target);
    request.setVersion(version);

    if (!splitTarget())
        return STEP_ERROR;

    parsedSize = lineEnd + 2;
    state = PARSE_HEADERS;

    return STEP_COMPLETE;
}


bool HttpRequestParser::storeHeader(const std::string &key, const std::string &value)
{
    std::string normalizedKey = toLower(key);
    std::string normalizedValue = trim(value);

    if ((normalizedKey == "host" || normalizedKey == "content-length") && request.hasHeader(normalizedKey))
    {
        setError(400);
        return false;
    }

    if (!isCommaSeparatedHeader(normalizedKey))
    {
        request.appendHeader(normalizedKey, normalizedValue);
        return true;
    }

    std::vector<std::string> values = splitHeaderValues(normalizedValue);

    if (values.empty())
    {
        request.appendHeader(normalizedKey, "");
        return true;
    }

    for (size_t i = 0; i < values.size(); ++i)
        request.appendHeader(normalizedKey, values[i]);

    return true;
}


StepStatus HttpRequestParser::parseHeaders(const std::string &raw)
{
    size_t headersEnd = raw.find("\r\n\r\n", parsedSize);

    if (headersEnd == std::string::npos)
    {
        if (raw.size() - parsedSize > MAX_HEADER_SIZE)
        {
            setError(431);
            return STEP_ERROR;
        }
        return STEP_NEED_MORE_DATA;
    }
    if (headersEnd - parsedSize > MAX_HEADER_SIZE)
    {
        setError(431);
        return STEP_ERROR;
    }
    std::string headerSection = raw.substr(parsedSize, headersEnd - parsedSize);

    std::istringstream headerStream(headerSection);
    std::string line;

    while (std::getline(headerStream, line))
    {
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);

        if (line.empty())
            continue;

        size_t delimiterPosition = line.find(':');

        if (delimiterPosition == std::string::npos)
        {
            setError(400);
            return STEP_ERROR;
        }

        std::string key = line.substr(0, delimiterPosition);
        if (key.empty() || key != trim(key))
        {
            setError(400);
            return STEP_ERROR;
        }

        std::string value = trim(line.substr(delimiterPosition + 1));
        if (!storeHeader(key, value))
            return STEP_ERROR;
    }
    const std::string &version = request.getVersion();
    if (version != "HTTP/1.1" && version != "HTTP/1.0")
    {
        setError(505);
        return STEP_ERROR;
    }
    if (version == "HTTP/1.1")
    {
        const std::vector<std::string> &hostValues = request.getHeader("host");

        if (hostValues.size() != 1 || trim(hostValues[0]).empty())
        {
            setError(400);
            return STEP_ERROR;
        }
    }
    parsedSize = headersEnd + 4;
    state = PARSE_BODY;
    return STEP_COMPLETE;
}

StepStatus HttpRequestParser::parseChunkedBody(const std::string &raw)
{
    const std::string crlf = "\r\n";
    const size_t crlfSize = crlf.size();

    while (true)
    {
        size_t chunkHeaderEnd = raw.find(crlf, parsedSize);

        if (chunkHeaderEnd == std::string::npos)
            return STEP_NEED_MORE_DATA;

        std::string chunkHeader = raw.substr(parsedSize, chunkHeaderEnd - parsedSize);

        size_t extensionPosition = chunkHeader.find(';');
        if (extensionPosition != std::string::npos)
            chunkHeader = chunkHeader.substr(0, extensionPosition);

        chunkHeader = trim(chunkHeader);
        size_t chunkSize = 0;

        if (!parseHexSize(chunkHeader, chunkSize))
        {
            setError(400);
            return STEP_ERROR;
        }

        size_t dataStart = chunkHeaderEnd + crlfSize;
        if (chunkSize == 0)
        {
            if (raw.size() < dataStart + crlfSize)
                return STEP_NEED_MORE_DATA;

            if (raw.compare(dataStart, crlfSize, crlf) != 0)
            {
                setError(400);
                return STEP_ERROR;
            }

            parsedSize = dataStart + crlfSize;
            return STEP_COMPLETE;
        }

        if (maxBodySize > 0)
        {
            size_t currentBodySize = request.getBody().size();

            if (currentBodySize > maxBodySize
                || chunkSize > maxBodySize - currentBodySize)
            {
                setError(413);
                return STEP_ERROR;
            }
        }

        if (dataStart > raw.size())
            return STEP_NEED_MORE_DATA;

        if (chunkSize > raw.size() - dataStart)
            return STEP_NEED_MORE_DATA;

        size_t chunkEndingPosition = dataStart + chunkSize;

        if (raw.size() < chunkEndingPosition + crlfSize)
            return STEP_NEED_MORE_DATA;

        if (raw.compare(chunkEndingPosition, crlfSize, crlf) != 0)
        {
            setError(400);
            return STEP_ERROR;
        }

        request.appendBody(raw.substr(dataStart, chunkSize));
        parsedSize = chunkEndingPosition + crlfSize;
    }
}

StepStatus HttpRequestParser::parseBody(const std::string &raw)
{
    const std::vector<std::string> &contentLengthValues = request.getHeader("content-length");
    const std::vector<std::string> &transferEncodingValues = request.getHeader("transfer-encoding");

    bool hasContentLength = !contentLengthValues.empty();
    bool hasTransferEncoding = !transferEncodingValues.empty();

    if (hasContentLength && contentLengthValues.size() != 1)
    {
        setError(400);
        return STEP_ERROR;
    }

    if (hasContentLength && trim(contentLengthValues[0]).empty())
    {
        setError(400);
        return STEP_ERROR;
    }

    if (hasTransferEncoding && transferEncodingValues.size() == 1
        && trim(transferEncodingValues[0]).empty())
    {
        setError(400);
        return STEP_ERROR;
    }

    if (hasContentLength && hasTransferEncoding)
    {
        setError(400);
        return STEP_ERROR;
    }

    if (hasTransferEncoding)
    {
        for (size_t i = 0; i < transferEncodingValues.size(); ++i)
        {
            if (trim(transferEncodingValues[i]).empty())
            {
                setError(400);
                return STEP_ERROR;
            }
        }

        if (toLower(transferEncodingValues.back()) != "chunked")
        {
            setError(501);
            return STEP_ERROR;
        }

        for (size_t i = 0; i + 1 < transferEncodingValues.size(); ++i)
        {
            if (toLower(transferEncodingValues[i]) != "identity")
            {
                setError(501);
                return STEP_ERROR;
            }
        }

        StepStatus chunkStatus = parseChunkedBody(raw);

        if (chunkStatus != STEP_COMPLETE)
            return chunkStatus;
    }
    else if (hasContentLength)
    {
        std::string contentLengthHeader = trim(contentLengthValues[0]);
        size_t contentLength = 0;

        if (!parseDecimalSize(contentLengthHeader, contentLength))
        {
            setError(400);
            return STEP_ERROR;
        }

        if (maxBodySize > 0 && contentLength > maxBodySize)
        {
            setError(413);
            return STEP_ERROR;
        }

        size_t currentBodySize = request.getBody().size();

        if (currentBodySize > contentLength)
        {
            setError(400);
            return STEP_ERROR;
        }

        if (parsedSize > raw.size())
        {
            setError(400);
            return STEP_ERROR;
        }

        size_t neededBytes = contentLength - currentBodySize;
        size_t availableBytes = raw.size() - parsedSize;
        size_t copiedBytes = std::min(neededBytes, availableBytes);

        if (copiedBytes > 0)
        {
            request.appendBody(raw.substr(parsedSize, copiedBytes));
            parsedSize += copiedBytes;
        }

        if (request.getBody().size() < contentLength)
            return STEP_NEED_MORE_DATA;
    }

    state = PARSE_COMPLETE;
    return STEP_COMPLETE;
}

ParseStatus HttpRequestParser::parse(const std::string &rawRequestData)
{
    while (state != PARSE_COMPLETE && state != PARSE_ERROR)
    {
        State previousState = state;
        StepStatus stepStatus = STEP_ERROR;

        switch (state)
        {
            case PARSE_REQUEST_LINE:
                stepStatus = parseRequestLine(rawRequestData);
                break;

            case PARSE_HEADERS:
                stepStatus = parseHeaders(rawRequestData);
                break;

            case PARSE_BODY:
                stepStatus = parseBody(rawRequestData);
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
            return PARSE_HEADERS_COMPLETE;
    }

    if (state == PARSE_ERROR)
        return PARSE_REQUEST_ERROR;

    return PARSE_REQUEST_COMPLETE;
}
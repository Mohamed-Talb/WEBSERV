#include "HttpRequest.hpp"
#include "../Helpers.hpp"
#include "HttpUtils/HttpUtils.hpp"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>

namespace
{
    bool urlDecode(const std::string &input, std::string &output)
    {
        output.clear();
        output.reserve(input.size());

        for (size_t i = 0; i < input.size(); ++i)
        {
            if (input[i] != '%')
            {
                output += input[i];
                continue;
            }

            if (i + 2 >= input.size())
                return false;

            unsigned char first = static_cast<unsigned char>(input[i + 1]);
            unsigned char second = static_cast<unsigned char>(input[i + 2]);

            if (!std::isxdigit(first) || !std::isxdigit(second))
                return false;

            std::string hexValue = input.substr(i + 1, 2);
            std::istringstream stream(hexValue);

            int decodedValue = 0;
            stream >> std::hex >> decodedValue;

            if (stream.fail())
                return false;

            output += static_cast<char>(decodedValue);
            i += 2;
        }

        return true;
    }

    bool parseDecimalSize(const std::string &value, size_t &result)
    {
        if (value.empty())
            return false;

        for (size_t i = 0; i < value.size(); ++i)
        {
            unsigned char character = static_cast<unsigned char>(value[i]);

            if (!std::isdigit(character))
                return false;
        }

        std::istringstream stream(value);
        size_t parsedValue = 0;

        stream >> parsedValue;

        if (stream.fail() || !stream.eof())
            return false;

        result = parsedValue;
        return true;
    }

    bool parseHexSize(const std::string &value, size_t &result)
    {
        if (value.empty())
            return false;

        for (size_t i = 0; i < value.size(); ++i)
        {
            unsigned char character = static_cast<unsigned char>(value[i]);

            if (!std::isxdigit(character))
                return false;
        }

        std::istringstream stream(value);
        size_t parsedValue = 0;

        stream >> std::hex >> parsedValue;

        if (stream.fail() || !stream.eof())
            return false;

        result = parsedValue;
        return true;
    }
}

HttpRequest::HttpRequest() : maxBodySize(0), parsedSize(0), state(PARSE_REQUEST_LINE), errorCode(0) {}

HttpRequest::~HttpRequest() {}

void HttpRequest::setMaxBodySize(size_t value)
{
    maxBodySize = value;
}

const std::string &HttpRequest::getHeader(const std::string &key) const
{
    std::map<std::string, std::string>::const_iterator it =
        headers.find(toLower(key));

    if (it != headers.end())
        return it->second;

    static const std::string empty;
    return empty;
}

const std::string &HttpRequest::getBody() const
{
    return body;
}

const std::string &HttpRequest::getMethod() const
{
    return method;
}

const std::string &HttpRequest::getTarget() const
{
    return target;
}

const std::string &HttpRequest::getVersion() const
{
    return version;
}

const std::string &HttpRequest::getQuery() const
{
    return query;
}

const std::string &HttpRequest::getRequestPath() const
{
    return requestPath;
}

int HttpRequest::getErrorCode() const
{
    return errorCode;
}

size_t HttpRequest::getParsedSize() const
{
    return parsedSize;
}

bool HttpRequest::shouldCloseConnection() const
{
    std::string connection = toLower(getHeader("connection"));

    if (version == "HTTP/1.0")
        return connection != "keep-alive";

    return connection == "close";
}

void HttpRequest::setError(int code)
{
    errorCode = code;
    state = PARSE_ERROR;
}

bool HttpRequest::splitTarget()
{
    size_t queryPosition = target.find('?');
    std::string rawPath;

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

    if (!urlDecode(rawPath, requestPath))
    {
        setError(400);
        return false;
    }

    requestPath = normalizePath(requestPath);
    return true;
}

void HttpRequest::reset()
{
    method.clear();
    target.clear();
    version.clear();
    requestPath.clear();
    query.clear();
    body.clear();
    headers.clear();

    maxBodySize = 0;
    parsedSize = 0;
    errorCode = 0;
    state = PARSE_REQUEST_LINE;
}

StepStatus HttpRequest::parseRequestLine(const std::string &raw)
{
    size_t lineEnd = raw.find("\r\n", parsedSize);

    if (lineEnd == std::string::npos)
        return STEP_NEED_MORE_DATA;

    std::string line = raw.substr(parsedSize, lineEnd - parsedSize);
    std::istringstream lineStream(line);
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

    method = toUpper(method);

    if (!splitTarget())
        return STEP_ERROR;

    parsedSize = lineEnd + 2;
    state = PARSE_HEADERS;

    return STEP_COMPLETE;
}

StepStatus HttpRequest::parseHeaders(const std::string &raw)
{
    size_t headersEnd = raw.find("\r\n\r\n", parsedSize);

    if (headersEnd == std::string::npos)
        return STEP_NEED_MORE_DATA;

    std::string headerSection =
        raw.substr(parsedSize, headersEnd - parsedSize);

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

        std::string key =
            toLower(trim(line.substr(0, delimiterPosition)));

        std::string value =
            trim(line.substr(delimiterPosition + 1));

        if (key.empty())
        {
            setError(400);
            return STEP_ERROR;
        }

        if ((key == "content-length" || key == "host")
            && headers.count(key) != 0)
        {
            setError(400);
            return STEP_ERROR;
        }

        if (headers.count(key) != 0)
            headers[key] += ", " + value;
        else
            headers[key] = value;
    }

    if (version != "HTTP/1.1" && version != "HTTP/1.0")
    {
        setError(505);
        return STEP_ERROR;
    }

    if (version == "HTTP/1.1" && headers.count("host") == 0)
    {
        setError(400);
        return STEP_ERROR;
    }

    parsedSize = headersEnd + 4;
    state = PARSE_BODY;

    return STEP_COMPLETE;
}

StepStatus HttpRequest::parseChunkedBody(const std::string &raw)
{
    const std::string crlf = "\r\n";
    const size_t crlfSize = crlf.size();

    while (true)
    {
        size_t chunkHeaderEnd = raw.find(crlf, parsedSize);

        if (chunkHeaderEnd == std::string::npos)
            return STEP_NEED_MORE_DATA;

        std::string chunkHeader =
            raw.substr(parsedSize, chunkHeaderEnd - parsedSize);

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

        if (maxBodySize > 0 && chunkSize > maxBodySize - body.size())
        {
            setError(413);
            return STEP_ERROR;
        }

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

        body.append(raw, dataStart, chunkSize);
        parsedSize = chunkEndingPosition + crlfSize;
    }
}

StepStatus HttpRequest::parseBody(const std::string &raw)
{
    std::string transferEncoding =
        toLower(trim(getHeader("transfer-encoding")));

    std::string contentLengthHeader =
        trim(getHeader("content-length"));

    if (!transferEncoding.empty() && !contentLengthHeader.empty())
    {
        setError(400);
        return STEP_ERROR;
    }

    if (!transferEncoding.empty())
    {
        if (transferEncoding != "chunked")
        {
            setError(501);
            return STEP_ERROR;
        }

        StepStatus chunkStatus = parseChunkedBody(raw);

        if (chunkStatus != STEP_COMPLETE)
            return chunkStatus;
    }
    else if (!contentLengthHeader.empty())
    {
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

        if (body.size() > contentLength)
        {
            setError(400);
            return STEP_ERROR;
        }

        size_t neededBytes = contentLength - body.size();
        size_t availableBytes = raw.size() - parsedSize;
        size_t copiedBytes = std::min(neededBytes, availableBytes);

        if (copiedBytes > 0)
        {
            body.append(raw, parsedSize, copiedBytes);
            parsedSize += copiedBytes;
        }

        if (body.size() < contentLength)
            return STEP_NEED_MORE_DATA;
    }

    state = PARSE_COMPLETE;
    return STEP_COMPLETE;
}

ParseStatus HttpRequest::parse(const std::string &rawRequestData)
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
                return PARSE_REQUEST_COMPLETE;
        }

        if (stepStatus == STEP_NEED_MORE_DATA)
            return PARSE_NEED_MORE_DATA;

        if (stepStatus == STEP_ERROR)
            return PARSE_REQUEST_COMPLETE;

        if (previousState == PARSE_HEADERS && state == PARSE_BODY)
            return PARSE_HEADERS_COMPLETE;
    }

    return PARSE_REQUEST_COMPLETE;
}
#include "RequestParser.hpp"

#include <cctype>
#include <sstream>
#include <vector>


bool isValidHeaderName(const std::string &name)
{
    if (name.empty())
        return false;

    const std::string allowed = "!#$%&'*+-.^_`|~";

    for (size_t i = 0; i < name.size(); ++i)
    {
        unsigned char current = static_cast<unsigned char>(name[i]);

        if (std::isalnum(current))
            continue;

        if (allowed.find(name[i]) == std::string::npos)
            return false;
    }

    return true;
}

bool isValidHeaderValue(const std::string &value)
{
    for (size_t i = 0; i < value.size(); ++i)
    {
        unsigned char current = static_cast<unsigned char>(value[i]);

        if (current == '\r' || current == '\n' || current == 0x7f)
            return false;

        if (current < 0x20 && current != '\t')
            return false;
    }

    return true;
}

bool hasSingleValue(const HttpRequest &request, const std::string &name)
{
    return request.getRawHeader(name).size() == 1;
}

std::vector<std::string> splitValues(const std::vector<std::string> &headers, char delimiter)
{
    std::vector<std::string> result;

    for (size_t headerIndex = 0; headerIndex < headers.size(); ++headerIndex)
    {
        const std::string &header = headers[headerIndex];
        size_t start = 0;

        for (size_t i = 0; i <= header.size(); ++i)
        {
            if (i == header.size() || header[i] == delimiter)
            {
                std::string value = trim(header.substr(start, i - start));

                if (value.empty())
                    return std::vector<std::string>();

                result.push_back(value);
                start = i + 1;
            }
        }
    }

    return result;
}

std::string removeQuotes(const std::string &value)
{
    if (value.size() >= 2 && value[0] == '"' && value[value.size() - 1] == '"')
        return value.substr(1, value.size() - 2);

    return value;
}

int parseHost(HttpRequest &request)
{
    const std::vector<std::string> &values = request.getRawHeader("host");

    if (values.empty())
    {
        if (request.getVersion() == "HTTP/1.1")
            return 400;

        return 0;
    }

    if (values.size() != 1 || trim(values[0]).empty())
        return 400;

    request.setHost(trim(values[0]));
    return 0;
}

int parseContentLength(HttpRequest &request)
{
    if (!request.hasHeader("content-length"))
        return 0;

    const std::vector<std::string> &values = request.getRawHeader("content-length");

    if (values.size() != 1 || trim(values[0]).empty())
        return 400;

    size_t contentLength = 0;

    if (!parseDecimalSize(trim(values[0]), contentLength))
        return 400;

    request.setContentLength(contentLength);
    return 0;
}

int parseContentType(HttpRequest &request)
{
    if (!request.hasHeader("content-type"))
        return 0;

    if (!hasSingleValue(request, "content-type"))
        return 400;

    const std::string raw = trim(request.getRawHeader("content-type")[0]);

    if (raw.empty())
        return 400;

    std::vector<std::string> parts;
    size_t start = 0;

    for (size_t i = 0; i <= raw.size(); ++i)
    {
        if (i == raw.size() || raw[i] == ';')
        {
            std::string part = trim(raw.substr(start, i - start));

            if (part.empty())
                return 400;

            parts.push_back(part);
            start = i + 1;
        }
    }

    std::string mediaType = parts[0];
    size_t slashPosition = mediaType.find('/');

    if (mediaType.find(',') != std::string::npos)
        return 400;

    if (slashPosition == std::string::npos || slashPosition == 0 || slashPosition == mediaType.size() - 1)
        return 400;

    if (mediaType.find('/', slashPosition + 1) != std::string::npos)
        return 400;

    ContentTypeData data;

    data.raw = raw;
    data.mediaType = toLower(mediaType);

    for (size_t i = 1; i < parts.size(); ++i)
    {
        size_t equalPosition = parts[i].find('=');

        if (equalPosition == std::string::npos || equalPosition == 0)
            return 400;

        std::string name = toLower(trim(parts[i].substr(0, equalPosition)));
        std::string value = trim(parts[i].substr(equalPosition + 1));

        if (name.empty() || value.empty())
            return 400;

        if (data.parameters.find(name) != data.parameters.end())
            return 400;

        data.parameters[name] = removeQuotes(value);
    }

    request.setContentType(data);
    return 0;
}

int parseTransferEncoding(HttpRequest &request)
{
    if (!request.hasHeader("transfer-encoding"))
        return 0;

    std::vector<std::string> codings = splitValues(request.getRawHeader("transfer-encoding"), ',');

    if (codings.empty())
        return 400;

    if (codings.size() != 1 || toLower(codings[0]) != "chunked")
        return 501;

    request.setChunked(true);
    return 0;
}

int parseConnection(HttpRequest &request)
{
    bool closeConnection = request.getVersion() == "HTTP/1.0";
    if (!request.hasHeader("connection"))
    {
        request.setCloseConnection(closeConnection);
        return 0;
    }
    std::vector<std::string> tokens = splitValues(request.getRawHeader("connection"), ',');

    if (tokens.empty())
        return 400;

    bool hasClose = false;
    bool hasKeepAlive = false;

    for (size_t i = 0; i < tokens.size(); ++i)
    {
        std::string token = toLower(tokens[i]);

        if (token == "close")
            hasClose = true;
        else if (token == "keep-alive")
            hasKeepAlive = true;
    }

    if (hasClose)
        closeConnection = true;
    else if (hasKeepAlive)
        closeConnection = false;

    request.setCloseConnection(closeConnection);
    return 0;
}

int parseSupportedHeaders(HttpRequest &request)
{
    int error = parseHost(request);

    if (error != 0)
        return error;

    error = parseContentLength(request);

    if (error != 0)
        return error;

    error = parseContentType(request);

    if (error != 0)
        return error;

    error = parseTransferEncoding(request);

    if (error != 0)
        return error;

    error = parseConnection(request);

    if (error != 0)
        return error;

    if (request.hasContentLength() && request.isChunked())
        return 400;

    return 0;
}


StepStatus RequestParser::headersParser(const std::string &raw)
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

        if (line[0] == ' ' || line[0] == '\t')
        {
            setError(400);
            return STEP_ERROR;
        }

        size_t delimiterPosition = line.find(':');

        if (delimiterPosition == std::string::npos)
        {
            setError(400);
            return STEP_ERROR;
        }

        std::string name = line.substr(0, delimiterPosition);
        std::string value = trim(line.substr(delimiterPosition + 1));

        if (!isValidHeaderName(name) || !isValidHeaderValue(value))
        {
            setError(400);
            return STEP_ERROR;
        }
        request.appendHeader(toLower(name), value);
    }

    const std::string &version = request.getVersion();

    if (version != "HTTP/1.1" && version != "HTTP/1.0")
    {
        setError(505);
        return STEP_ERROR;
    }

    int error = parseSupportedHeaders(request);

    if (error != 0)
    {
        setError(error);
        return STEP_ERROR;
    }

    parsedSize = headersEnd + 4;
    state = PARSE_BODY;

    return STEP_COMPLETE;
}
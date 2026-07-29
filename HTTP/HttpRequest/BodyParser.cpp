#include "RequestParser.hpp"


StepStatus RequestParser::parseNormalBody(const std::string &raw)
{
    if (!bodySizeInitialized)
    {
        expectedBodySize = request.getContentLength();

        if (maxBodySize > 0 && expectedBodySize > maxBodySize)
        {
            setError(413);
            return STEP_ERROR;
        }

        request.reserveBody(expectedBodySize);
        bodySizeInitialized = true;
    }

    size_t currentBodySize = request.getBody().size();

    if (currentBodySize > expectedBodySize || parsedSize > raw.size())
    {
        setError(400);
        return STEP_ERROR;
    }

    size_t neededBytes = expectedBodySize - currentBodySize;
    size_t availableBytes = raw.size() - parsedSize;
    size_t copiedBytes = std::min(neededBytes, availableBytes);

    if (copiedBytes > 0)
    {
        request.appendToBody(raw.data() + parsedSize, copiedBytes);
        parsedSize += copiedBytes;
    }

    if (request.getBody().size() < expectedBodySize)
        return STEP_NEED_MORE_DATA;

    return STEP_COMPLETE;
}

StepStatus RequestParser::parseChunkedBody(const std::string &raw)
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

        size_t currentBodySize = request.getBody().size();

        if (maxBodySize > 0 && (currentBodySize > maxBodySize || chunkSize > maxBodySize - currentBodySize))
        {
            setError(413);
            return STEP_ERROR;
        }

        if (dataStart > raw.size() || chunkSize > raw.size() - dataStart)
            return STEP_NEED_MORE_DATA;

        size_t chunkEnd = dataStart + chunkSize;

        if (raw.size() < chunkEnd + crlfSize)
            return STEP_NEED_MORE_DATA;

        if (raw.compare(chunkEnd, crlfSize, crlf) != 0)
        {
            setError(400);
            return STEP_ERROR;
        }

        request.appendToBody(raw.data() + dataStart, chunkSize);
        parsedSize = chunkEnd + crlfSize;
    }
}

StepStatus RequestParser::bodyParser(const std::string &raw)
{
    StepStatus status = STEP_COMPLETE;

    if (request.isChunked())
        status = parseChunkedBody(raw);
    else if (request.hasContentLength())
        status = parseNormalBody(raw);

    if (status != STEP_COMPLETE)
        return status;

    state = PARSE_COMPLETE;
    return STEP_COMPLETE;
}
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
    const size_t crlfSize = 2;
    const size_t maxChunkHeaderSize = 1024;
    while (true)
    {
        if (parsedSize > raw.size())
        {
            setError(400);
            return STEP_ERROR;
        }
        size_t chunkHeaderEnd = raw.find("\r\n", parsedSize);
        if (chunkHeaderEnd == std::string::npos)
        {
            if (raw.size() - parsedSize > maxChunkHeaderSize)
            {
                setError(400);
                return STEP_ERROR;
            }
            return STEP_NEED_MORE_DATA;
        }
        if (chunkHeaderEnd - parsedSize > maxChunkHeaderSize)
        {
            setError(400);
            return STEP_ERROR;
        }

        std::string chunkHeader = raw.substr(parsedSize, chunkHeaderEnd - parsedSize);

        size_t extensionPosition = chunkHeader.find(';');
        if (extensionPosition != std::string::npos)
            chunkHeader.erase(extensionPosition);

        chunkHeader = trim(chunkHeader);

        size_t chunkSize = 0;
        if (!parseHexSize(chunkHeader, chunkSize))
        {
            setError(400);
            return STEP_ERROR;
        }

        const size_t dataStart = chunkHeaderEnd + crlfSize;
        if (chunkSize == 0)
        {
            if (dataStart > raw.size() || raw.size() - dataStart < crlfSize)
                return STEP_NEED_MORE_DATA;
            if (raw.compare(dataStart, crlfSize, "\r\n") != 0)
            {
                setError(400);
                return STEP_ERROR;
            }
            parsedSize = dataStart + crlfSize;
            return STEP_COMPLETE;
        }
        const size_t currentBodySize = request.getBody().size();
        if (maxBodySize > 0)
        {
            if (currentBodySize > maxBodySize || chunkSize > maxBodySize - currentBodySize)
            {
                setError(413);
                return STEP_ERROR;
            }
        }
        if (dataStart > raw.size() || chunkSize > raw.size() - dataStart)
            return STEP_NEED_MORE_DATA;

        const size_t chunkEnd = dataStart + chunkSize;
        if (raw.size() - chunkEnd < crlfSize)
            return STEP_NEED_MORE_DATA;

        if (raw.compare(chunkEnd, crlfSize, "\r\n") != 0)
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
    StepStatus status;

    if (request.isChunked())
        status = parseChunkedBody(raw);
    else if (request.hasContentLength())
        status = parseNormalBody(raw);
    else
        status = STEP_COMPLETE;

    if (status != STEP_COMPLETE)
        return status;

    state = PARSE_COMPLETE;
    return STEP_COMPLETE;
}
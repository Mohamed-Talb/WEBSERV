
#include "HttpHandler.hpp"

HttpRequest::HttpRequest() : state(PARSE_REQUEST_LINE), consumedBytes(0), errorCode(0) {}
void HttpRequest::reset() 
{
    method.clear(); target.clear(); version.clear();
    headers.clear(); body.clear();
    consumedBytes = 0; 
    errorCode = 0;
    state = PARSE_REQUEST_LINE;
}

static bool parseChunkedBody(const std::string& rawInputData, std::string& decodedBody, size_t& totalConsumed)
{
    decodedBody.clear();
    totalConsumed = 0;
    
    size_t currentPosition = 0;
    const size_t CRLF_SIZE = 2;
    const std::string CRLF = "\r\n";

    while (true)
    {
        size_t chunkHeaderEndPos = rawInputData.find(CRLF, currentPosition);
        if (chunkHeaderEndPos == std::string::npos)
            return false;

        std::string chunkHeader = rawInputData.substr(currentPosition, chunkHeaderEndPos - currentPosition);
        size_t extensionStartPos = chunkHeader.find(';');
        if (extensionStartPos != std::string::npos)
            chunkHeader = chunkHeader.substr(0, extensionStartPos);

        size_t dataChunkSize = 0;
        std::istringstream hexStream(chunkHeader);
        hexStream >> std::hex >> dataChunkSize;

        if (hexStream.fail() || !hexStream.eof())
            return false;
        currentPosition = chunkHeaderEndPos + CRLF_SIZE;

        if (rawInputData.size() < currentPosition + dataChunkSize + CRLF_SIZE)
            return false;

        if (dataChunkSize == 0)
        {
            if (rawInputData.compare(currentPosition, CRLF_SIZE, CRLF) != 0)
                return false;
            totalConsumed = currentPosition + CRLF_SIZE;
            return true;
        }
        decodedBody.append(rawInputData, currentPosition, dataChunkSize);
        currentPosition += dataChunkSize;

        if (rawInputData.compare(currentPosition, CRLF_SIZE, CRLF) != 0)
            return false;

        currentPosition += CRLF_SIZE;
    }
}

HttpRequest::~HttpRequest() {}


int HttpRequest::extractBody(const std::string& rawBodyData, size_t& bytesConsumed)
{
    // RETURN VALUES:
    // 1 → success (body fully parsed)
    // 0 → need more data (incomplete)
    // -1 → error (invalid request)
    std::map<std::string, std::string>::iterator te_it = headers.find("transfer-encoding");
    std::map<std::string, std::string>::iterator cl_it = headers.find("content-length");
    if (te_it != headers.end() && cl_it != headers.end())
        return -1;
    if (te_it != headers.end())
    {
        if (toLower(te_it->second) != "chunked")
            return -1;
        if (!parseChunkedBody(rawBodyData, body, bytesConsumed))
            return 0;
        return 1;
    }
    else if (cl_it != headers.end())
    {
        std::string lengthString = cl_it->second;
        if (lengthString.empty())
            return -1;

        std::istringstream sizeStream(lengthString);
        size_t contentLength = 0;
        sizeStream >> contentLength;

        if (sizeStream.fail() || !sizeStream.eof())
            return -1;

        if (rawBodyData.size() < contentLength)
            return 0;
        body = rawBodyData.substr(0, contentLength);
        bytesConsumed = contentLength;
    }
    return 1;
}


bool HttpRequest::parseHeaders(std::istringstream &input)
{
    std::string line;
    while (std::getline(input, line))
    {
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);
            
        if (line.empty())
            continue;

        size_t delimiterPos = line.find(':');
        if (delimiterPos == std::string::npos)
            return false;

        std::string headerKey = toLower(trim(line.substr(0, delimiterPos)));
        std::string headerValue = trim(line.substr(delimiterPos + 1));
        headers[headerKey] = headerValue;
    }
    return true;
}

bool HttpRequest::parseRequestLine(std::istringstream &input)
{
    std::string line;
    if (!std::getline(input, line))
        return false;

    if (!line.empty() && line[line.size() - 1] == '\r')
        line.erase(line.size() - 1);

    std::istringstream lineStream(line);
    if (!(lineStream >> method >> target >> version))
        return false;

    method = toUpper(method);
    return true;
}

void HttpRequest::setError(int code)
{
    errorCode = code;
    state = PARSE_ERROR;
}

int HttpRequest::parse(const std::string &rawRequestData)
{
    while (state != PARSE_COMPLETE && state != PARSE_ERROR)
    {
        if (state == PARSE_REQUEST_LINE)
        {
            size_t crlf = rawRequestData.find("\r\n", consumedBytes);
            if (crlf == std::string::npos) return 0; // Need more data

            std::string line = rawRequestData.substr(consumedBytes, crlf - consumedBytes);
            std::istringstream lineStream(line);
            
            if (!(lineStream >> method >> target >> version)) 
			{
                setError(400);
                return 1; 
            }
            consumedBytes = crlf + 2;
            state = PARSE_HEADERS;
        }
        else if (state == PARSE_HEADERS)
        {
            size_t headerEnd = rawRequestData.find("\r\n\r\n", consumedBytes);
            if (headerEnd == std::string::npos) return 0; // Need more data

            std::string headerSection = rawRequestData.substr(consumedBytes, headerEnd - consumedBytes);
            std::istringstream headerStream(headerSection);
            
            if (!parseHeaders(headerStream)) 
			{
                setError(400);
                return 1;
            }
            
            consumedBytes = headerEnd + 4;
            state = PARSE_BODY;
        }
        else if (state == PARSE_BODY)
        {
            std::string bodyData = rawRequestData.substr(consumedBytes);
            size_t bodyConsumed = 0;
            int status = extractBody(bodyData, bodyConsumed);
            
            if (status == 0) return 0;
            if (status == -1) {
                setError(400);
                return 1;
            }
            
            consumedBytes += bodyConsumed;
            state = PARSE_COMPLETE;
        }
    }
    return 1;
}


std::string HttpRequest::getMethod() const { return method; }
std::string HttpRequest::getTarget() const { return target; }
std::string HttpRequest::getVersion() const { return version; }
std::string HttpRequest::getBody() const { return body; }
size_t HttpRequest::getConsumedBytes() const { return consumedBytes; }
int HttpRequest::getErrorCode() const { return errorCode; }

std::string HttpRequest::getHeader(const std::string& key) const
{
    std::map<std::string, std::string>::const_iterator it = headers.find(key);
    if (it != headers.end())
        return it->second;
    return "";
}

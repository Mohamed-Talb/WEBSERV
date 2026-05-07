#include "HttpRequest.hpp"
#include "../Helpers.hpp"


HttpRequest::HttpRequest() : maxBodySize(0), state(PARSE_REQUEST_LINE), parsedSize(0), errorCode(0) {}
HttpRequest::~HttpRequest() {}

void HttpRequest::setMaxBodySize(size_t value)
{
    maxBodySize = value;
}

const std::string &HttpRequest::getHeader(const std::string& key) const
{
    std::map<std::string, std::string>::const_iterator it = headers.find(key);
    if (it != headers.end())
        return it->second;
    static const std::string empty = "";
    return empty;
}

void HttpRequest::spliteTarget()
{
    size_t pos = target.find('?');
    if (pos == std::string::npos)
    {
        requestPath = target;
        querys.clear();
        return;
    }
    requestPath = target.substr(0, pos);
    querys = target.substr(pos + 1);
}

void HttpRequest::setError(int code)
{
    errorCode = code;
    state = PARSE_ERROR;
}

const std::string &HttpRequest::getBody()  const { return body; }
const std::string &HttpRequest::getMethod() const { return method; }
const std::string &HttpRequest::getTarget() const { return target; }
const std::string &HttpRequest::getVersion() const { return version; }
const std::string &HttpRequest::getQuery() const {return querys; }
const std::string &HttpRequest::getRequestPath() const { return requestPath;}
int HttpRequest::getErrorCode()	const { return errorCode; }
size_t HttpRequest::getParsedSize()	const { return parsedSize; }
State  HttpRequest::getState() const { return state; }


void HttpRequest::reset() 
{
    method.clear(); target.clear(); version.clear();
    headers.clear(); body.clear();
    requestPath.clear(); querys.clear();
    parsedSize = 0; 
    errorCode = 0;
    state = PARSE_REQUEST_LINE;
}

bool HttpRequest::parseChunkedBody(const std::string &rawInputData, std::string &decodedBody, size_t &totalConsumed)
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
        if (decodedBody.size() + dataChunkSize > maxBodySize) 
        {
            setError(413);
            return false; 
        }
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

int HttpRequest::parseRequestLine(const std::string &raw)
{
    size_t crlf = raw.find("\r\n", parsedSize);
    if (crlf == std::string::npos) 
		return 0;
    std::string line = raw.substr(parsedSize, crlf - parsedSize);
    std::istringstream lineStream(line);
    
    if (!(lineStream >> method >> target >> version)) 
    {
        setError(400);
        return -1; 
    }
    method = toUpper(method);
    parsedSize = crlf + 2;
    state = PARSE_HEADERS;
    spliteTarget();
    return 1;
}

int HttpRequest::parseHeaders(const std::string &raw)
{
    size_t headerEnd = raw.find("\r\n\r\n", parsedSize);
    if (headerEnd == std::string::npos) return 0; 

    std::string headerSection = raw.substr(parsedSize, headerEnd - parsedSize);
    std::istringstream headerStream(headerSection);
    
    std::string line;
    while (std::getline(headerStream, line))
    {
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);
            
        if (line.empty())
            continue;

        size_t delimiterPos = line.find(':');
        if (delimiterPos == std::string::npos)
        {
            setError(400);
            return -1;
        }
        std::string headerKey = toLower(trim(line.substr(0, delimiterPos)));
        std::string headerValue = trim(line.substr(delimiterPos + 1));
        headers[headerKey] = headerValue;
    }
    
    parsedSize = headerEnd + 4;
    state = PARSE_BODY;
    return 1; 
}

int HttpRequest::parseBody(const std::string &raw)
{
    size_t bodyConsumed = 0;

    std::map<std::string, std::string>::iterator te_it = headers.find("transfer-encoding");
    std::map<std::string, std::string>::iterator cl_it = headers.find("content-length");

    if (te_it != headers.end() && cl_it != headers.end())
    {
        setError(400);
        return -1;
    }
    if (te_it != headers.end())
    {
        if (toLower(te_it->second) != "chunked")
        {
            setError(400);
            return -1;
        }
        std::string rawBodyData(raw, parsedSize);
        if (!parseChunkedBody(rawBodyData, body, bodyConsumed))
            return 0;
    }
    else if (cl_it != headers.end())
    {
        const std::string &lengthString = cl_it->second;
        if (lengthString.empty())
        {
            setError(400);
            return -1;
        }
        std::istringstream sizeStream(lengthString);
        size_t contentLength = 0;
        sizeStream >> contentLength;
        if (sizeStream.fail() || !sizeStream.eof())
        {
            setError(400);
            return -1;
        }

        if (raw.size() - parsedSize < contentLength)
            return 0;
        body.assign(raw.data() + parsedSize, contentLength);
        bodyConsumed = contentLength;
    }
    parsedSize += bodyConsumed;
    state = PARSE_COMPLETE;
    return 1;
}

int HttpRequest::parse(const std::string &rawRequestData)
{
    while (state != PARSE_COMPLETE && state != PARSE_ERROR)
    {
        State prevState = state; 
        int status = 0;
        
        switch (state) 
        {
            case PARSE_REQUEST_LINE: status = parseRequestLine(rawRequestData); break;
            case PARSE_HEADERS:      status = parseHeaders(rawRequestData); break;
            case PARSE_BODY:         status = parseBody(rawRequestData); break;
            default:                 setError(500); return -1;
        }
        if (status <= 0) return status; 
        if (prevState == PARSE_HEADERS && state == PARSE_BODY) {
            return 2; 
        }
    }
    return 1;
}
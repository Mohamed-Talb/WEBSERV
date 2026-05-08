#include "HttpRequest.hpp"
#include "../Helpers.hpp"

HttpRequest::HttpRequest() : maxBodySize(0), state(PARSE_REQUEST_LINE), parsedSize(0), errorCode(0) {}
HttpRequest::~HttpRequest() {}
void HttpRequest::setMaxBodySize(size_t value) {maxBodySize = value;}


const std::string &HttpRequest::getHeader(const std::string &key) const 
{
    std::map<std::string, std::string>::const_iterator it = headers.find(toLower(key));
    if (it != headers.end())
    {
        return it->second;
    }
    static const std::string empty = "";
    return empty;
}


bool urlDecode(const std::string &in, std::string &out) 
{
    out.clear();
    out.reserve(in.length());
    for (std::size_t i = 0; i < in.length(); ++i) 
    {
        if (in[i] == '%') 
        {
            if (i + 2 < in.length()) 
            {
                if (std::isxdigit(in[i + 1]) && std::isxdigit(in[i + 2])) 
                {
                    std::string hexStr = in.substr(i + 1, 2);
                    std::istringstream hexStream(hexStr);
                    int hexVal;
                    hexStream >> std::hex >> hexVal;
                    out += static_cast<char>(hexVal);
                    i += 2;
                } 
                else 
                    return false;
            } 
            else 
                return false;
        } 
        else 
            out += in[i];
    }
    return true;
}

bool HttpRequest::spliteTarget() 
{
    size_t pos = target.find('?');
    std::string rawPath;
    if (pos == std::string::npos) 
    {
        rawPath = target;
        querys.clear();
    } 
    else 
    {
        rawPath = target.substr(0, pos);
        querys = target.substr(pos + 1);
    }
    if (!urlDecode(rawPath, requestPath)) 
    {
        setError(400);
        return false;
    }
    return true;
}


void HttpRequest::setError(int code) 
{
    errorCode = code;
    state = PARSE_ERROR;
}

const std::string &HttpRequest::getBody() const { return body; }
const std::string &HttpRequest::getMethod() const { return method; }
const std::string &HttpRequest::getTarget() const { return target; }
const std::string &HttpRequest::getVersion() const { return version; }
const std::string &HttpRequest::getQuery() const { return querys; }
const std::string &HttpRequest::getRequestPath() const { return requestPath; }
int HttpRequest::getErrorCode() const { return errorCode; }
size_t HttpRequest::getParsedSize() const { return parsedSize; }
State HttpRequest::getState() const { return state; }


void HttpRequest::reset() 
{
    method.clear(); target.clear(); version.clear();
    headers.clear(); body.clear();
    requestPath.clear(); querys.clear();
    parsedSize = 0; errorCode = 0;
    state = PARSE_REQUEST_LINE;
}


// true (full chunked body found), false (incomplete data or error)
bool HttpRequest::parseChunkedBody(const std::string &raw, std::string &decodedBody, size_t &totalConsumed, size_t startPos) 
{
    decodedBody.clear();
    totalConsumed = 0;
    size_t currentPosition = startPos;
    const size_t CRLF_SIZE = 2;
    const std::string CRLF = "\r\n";
    while (true) 
    {
        size_t chunkHeaderEndPos = raw.find(CRLF, currentPosition);
        if (chunkHeaderEndPos == std::string::npos) 
        {
            return false;
        }
        std::string chunkHeader = raw.substr(currentPosition, chunkHeaderEndPos - currentPosition);
        size_t extensionStartPos = chunkHeader.find(';');
        if (extensionStartPos != std::string::npos)
        {
            chunkHeader = chunkHeader.substr(0, extensionStartPos);
        }
        size_t dataChunkSize = 0;
        std::istringstream hexStream(chunkHeader);
        hexStream >> std::hex >> dataChunkSize;
        if (hexStream.fail() || !hexStream.eof()) 
        {
            return false;
        }
        currentPosition = chunkHeaderEndPos + CRLF_SIZE;
        if (raw.size() < currentPosition + dataChunkSize + CRLF_SIZE) 
        {
            return false;
        }
        if (decodedBody.size() + dataChunkSize > maxBodySize) 
        {
            setError(413);
            return false;
        }
        if (dataChunkSize == 0) 
        {
            if (raw.compare(currentPosition, CRLF_SIZE, CRLF) != 0) 
            {
                return false;
            }
            totalConsumed = (currentPosition + CRLF_SIZE) - startPos;
            return true;
        }
        decodedBody.append(raw, currentPosition, dataChunkSize);
        currentPosition += dataChunkSize;
        if (raw.compare(currentPosition, CRLF_SIZE, CRLF) != 0) 
        {
            return false;
        }
        currentPosition += CRLF_SIZE;
    }
}


// return: 1 (success), 0 (incomplete CRLF), -1 (syntax error)
int HttpRequest::parseRequestLine(const std::string &raw) 
{
    size_t crlf = raw.find("\r\n", parsedSize);
    if (crlf == std::string::npos) 
    {
        return 0;
    }
    std::string extraGarbage;
    std::string line = raw.substr(parsedSize, crlf - parsedSize);
    std::istringstream lineStream(line);
    if (!(lineStream >> method >> target >> version) || (lineStream >> extraGarbage)) 
    {
        setError(400);
        return -1;
    }
    if (target[0] != '/') 
    {
        setError(400);
        return -1;
    }
    method = toUpper(method);
    if (!spliteTarget()) 
    {
        return -1;
    }
    parsedSize = crlf + 2;
    state = PARSE_HEADERS;
    return 1;
}


// return: 1 (success), 0 (incomplete headers), -1 (syntax error)
int HttpRequest::parseHeaders(const std::string &raw) 
{
    size_t headerEnd = raw.find("\r\n\r\n", parsedSize);
    if (headerEnd == std::string::npos) 
    {
        return 0;
    }
    std::string headerSection = raw.substr(parsedSize, headerEnd - parsedSize);
    std::istringstream headerStream(headerSection);
    std::string line;
    while (std::getline(headerStream, line)) 
    {
        if (!line.empty() && line[line.size() - 1] == '\r') 
        {
            line.erase(line.size() - 1);
        }
        if (line.empty()) 
        {
            continue;
        }
        size_t delimiterPos = line.find(':');
        if (delimiterPos == std::string::npos) 
        {
            setError(400);
            return -1;
        }
        std::string key = toLower(trim(line.substr(0, delimiterPos)));
        std::string val = trim(line.substr(delimiterPos + 1));
        if ((key == "content-length" || key == "host") && headers.count(key)) 
        {
            setError(400);
            return -1;
        }
        if (headers.count(key)) 
        {
            headers[key] += ", " + val;
        }
        else 
        {
            headers[key] = val;
        }
    }
    parsedSize = headerEnd + 4;
    state = PARSE_BODY;
    return 1;
}


// return: 1 (success), 0 (more data needed), -1 (error/413)
int HttpRequest::parseBody(const std::string &raw) 
{
    size_t bodyConsumed = 0;
    std::string te = toLower(getHeader("transfer-encoding"));
    std::string cl = getHeader("content-length");
    if (!te.empty() && !cl.empty()) 
    { 
        setError(400); return -1; 
    }
    if (te == "chunked") 
    {
        if (!parseChunkedBody(raw, body, bodyConsumed, parsedSize)) 
        {
            return 0;
        }
    } 
    else if (!cl.empty()) 
    {
        size_t contentLength = static_cast<size_t>(std::atoll(cl.c_str()));
        if (raw.size() - parsedSize < contentLength) 
        {
            return 0;
        }
        body.clear();
        body.reserve(contentLength);
        body.append(raw, parsedSize, contentLength);
        bodyConsumed = contentLength;
    }
    parsedSize += bodyConsumed;
    state = PARSE_COMPLETE;
    return 1;
}


// return: 1 (Fully Parsed), 0 (Needs More Data), -1 (Fatal Error), 2 (Headers Finished/Body Next)
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
        if (status <= 0) 
            return status;
        if (prevState == PARSE_HEADERS && state == PARSE_BODY) 
            return 2;
    }
    return 1;
}
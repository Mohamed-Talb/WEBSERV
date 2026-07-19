#include "HttpRequest.hpp"
#include "../Helpers.hpp"
#include "HttpUtils/HttpUtils.hpp"

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
    requestPath = normalizePath(requestPath);
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
void  HttpRequest::setParsedSize(size_t size) {parsedSize = size;}


void HttpRequest::reset() 
{
    method.clear(); target.clear(); version.clear();
    headers.clear(); body.clear();
    requestPath.clear(); querys.clear();
    parsedSize = 0; errorCode = 0;
    state = PARSE_REQUEST_LINE;
}

void HttpRequest::cleanup(std::string &buffer)
{
    if (parsedSize > 0)
    {
        if (parsedSize >= buffer.size())
            buffer.clear();
        else
            buffer.erase(0, parsedSize);
    }
    reset();
}

// return: 1 (final chunk found), 0 (needs more data), -1 (error)
int HttpRequest::parseChunkedBody(const std::string &raw) 
{
    const size_t CRLF_SIZE = 2;
    const std::string CRLF = "\r\n";

    while (true) 
    {
        size_t chunkHeaderEndPos = raw.find(CRLF, parsedSize);
        if (chunkHeaderEndPos == std::string::npos) 
        {
            return 0;
        }

        std::string chunkHeader = raw.substr(parsedSize, chunkHeaderEndPos - parsedSize);
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
            setError(400);
            return -1;
        }
        size_t dataStartPos = chunkHeaderEndPos + CRLF_SIZE;
        size_t totalChunkBlockSize = (dataStartPos - parsedSize) + dataChunkSize + CRLF_SIZE;
        if (raw.size() - parsedSize < totalChunkBlockSize) 
        {
            return 0;
        }
        if (maxBodySize > 0 && body.size() + dataChunkSize > maxBodySize) 
        {
            setError(413);
            return -1;
        }
        if (dataChunkSize == 0) 
        {
            if (raw.compare(dataStartPos, CRLF_SIZE, CRLF) != 0) 
            {
                setError(400);
                return -1;
            }
            parsedSize += totalChunkBlockSize;
            return 1;
        }
        if (raw.compare(dataStartPos + dataChunkSize, CRLF_SIZE, CRLF) != 0) 
        {
            setError(400);
            return -1;
        }
        body.append(raw, dataStartPos, dataChunkSize);
        parsedSize += totalChunkBlockSize; 
        
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
    if (version != "HTTP/1.1" && version != "HTTP/1.0")
    {
        setError(505); 
        return -1;
    }
    if (version == "HTTP/1.1" && headers.find("host") == headers.end())
    {
        setError(400);
        return -1;
    }
    parsedSize = headerEnd + 4;
    state = PARSE_BODY;
    return 1;
}

int HttpRequest::parseBody(const std::string &raw) 
{
    std::string te = toLower(getHeader("transfer-encoding"));
    std::string cl = getHeader("content-length");
    
    if (!te.empty() && !cl.empty()) /// ?????????????
    { 
        setError(400); 
        return -1; 
    }
    
    if (te == "chunked") 
    {
        int status = parseChunkedBody(raw);
        if (status <= 0) return status; 
    } 
    else if (!cl.empty()) 
    {
        size_t contentLength = 0;
        std::istringstream iss(cl);
        iss >> contentLength;

        if (maxBodySize > 0 && contentLength > maxBodySize) {
            setError(413);
            return -1;
        }
        size_t needed = contentLength - body.size();
        size_t available = raw.size() - parsedSize;
        size_t toCopy = std::min(needed, available);

        if (toCopy > 0) 
        {
            body.append(raw, parsedSize, toCopy);
            parsedSize += toCopy;
        }

        if (body.size() < contentLength)
            return 0;
    }
    
    state = PARSE_COMPLETE;
    return 1;
}

// return: 1 (Fully Parsed), 0 (Needs More Data), -1 (Fatal Error), 2 (Headers Finished/Body Next)
ParseResult HttpRequest::parse(const std::string &rawRequestData) 
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
            default:                 setError(500); return RESULT_ERROR;
        }
        if (status <= -1) 
            return RESULT_ERROR;
        if (status <= 0) 
            return RESULT_NEED_MORE;
        if (prevState == PARSE_HEADERS && state == PARSE_BODY) 
            return RESULT_HEADERS_DONE;
    }
    return RESULT_COMPLETE;
}
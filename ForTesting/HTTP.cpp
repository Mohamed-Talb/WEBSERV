
#include "HttpRequest.hpp"
#include <cctype>

static std::string toUpper(std::string value)
{
    for (size_t i = 0; i < value.size(); ++i)
        value[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(value[i])));
    return value;
}

static std::string toLower(std::string value)
{
    for (size_t i = 0; i < value.size(); ++i)
        value[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(value[i])));
    return value;
}

static std::string trim(const std::string& value)
{
    size_t start = 0;
    while (start < value.size() && (value[start] == ' ' || value[start] == '\t'))
        ++start;
    size_t end = value.size();
    while (end > start && (value[end - 1] == ' ' || value[end - 1] == '\t'))
        --end;
    return value.substr(start, end - start);
}

#include "HttpRequest.hpp"

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




// HttpResponse.cpp
#include "HttpResponse.hpp"

HttpResponse::HttpResponse() : statusCode(200), reasonPhrase("OK")
{
    headers["Connection"] = "close";
}

HttpResponse::HttpResponse(int code, const std::string& reason) : statusCode(code), reasonPhrase(reason)
{
    headers["Connection"] = "close";
}

HttpResponse::~HttpResponse() {}

void HttpResponse::setHeader(const std::string& name, const std::string& value)
{
    headers[name] = value;
}

void HttpResponse::setBody(const std::string& content, const std::string& contentType)
{
    body = content;
    headers["Content-Type"] = contentType;

    std::ostringstream sizeStream;
    sizeStream << body.size();
    headers["Content-Length"] = sizeStream.str();
}

std::string HttpResponse::toString() const
{
    std::ostringstream responseStream;
    responseStream << "HTTP/1.1 " << statusCode << " " << reasonPhrase << "\r\n";

    std::map<std::string, std::string>::const_iterator headerIterator;
    for (headerIterator = headers.begin(); headerIterator != headers.end(); ++headerIterator)
    {
        responseStream << headerIterator->first << ": " << headerIterator->second << "\r\n";
    }
    responseStream << "\r\n" << body;
    return responseStream.str();
}




// HttpHandler.cpp
#include "HttpHandler.hpp"
#include <fstream>
#include <cctype>

HttpHandler::HttpHandler() {}
HttpHandler::~HttpHandler() {}

static std::string toUpper(std::string value)
{
    for (size_t i = 0; i < value.size(); ++i)
        value[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(value[i])));
    return value;
}

static std::string toLower(std::string value)
{
    for (size_t i = 0; i < value.size(); ++i)
        value[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(value[i])));
    return value;
}

std::string HttpHandler::detectContentType(const std::string& path)
{
    size_t extensionPos = path.find_last_of('.');
    if (extensionPos == std::string::npos)
        return "application/octet-stream";

    std::string fileExtension = toLower(path.substr(extensionPos));

    if (fileExtension == ".html" || fileExtension == ".htm") return "text/html";
    if (fileExtension == ".css") return "text/css";
    if (fileExtension == ".js") return "application/javascript";
    if (fileExtension == ".json") return "application/json";
    if (fileExtension == ".txt") return "text/plain";
    if (fileExtension == ".png") return "image/png";
    if (fileExtension == ".jpg" || fileExtension == ".jpeg") return "image/jpeg";
    if (fileExtension == ".gif") return "image/gif";

    return "application/octet-stream";
}

bool HttpHandler::readFile(const std::string& filePath, std::string& content)
{
    std::ifstream file(filePath.c_str(), std::ios::in | std::ios::binary);
    if (!file.is_open())
        return false;

    std::ostringstream fileBuffer;
    fileBuffer << file.rdbuf();
    content = fileBuffer.str();
    return true;
}

HttpResponse HttpHandler::buildError(int statusCode, const std::string& statusReason, const std::string& detail)
{
    HttpResponse response(statusCode, statusReason);
    response.setBody(detail, "text/plain");
    return response;
}

HttpResponse HttpHandler::process(const HttpRequest& request, const ServerConfig& config)
{

    int errorCode = request.getErrorCode();
    if (errorCode != 0)
    {
        if (errorCode == 505) 
            return buildError(505, "HTTP Version Not Supported", "Unsupported HTTP version");
        if (errorCode == 400)
            return buildError(400, "Bad Request", "Malformed request");
        return buildError(errorCode, "Error", "Parsing failed");
    }


    std::string method = request.getMethod();
    if (method != "GET" && method != "POST" && method != "DELETE")
        return buildError(405, "Method Not Allowed", "Unsupported method");

    std::string requestPath = request.getTarget();
    size_t queryStartPos = requestPath.find('?');
    if (queryStartPos != std::string::npos)
        requestPath = requestPath.substr(0, queryStartPos);

    const Location *bestLocationMatch = NULL;
    size_t longestMatchLen = 0;

    for (size_t i = 0; i < config.Locations.size(); ++i)
    {
        const Location &candidateLocation = config.Locations[i];
        
        bool isPrefixMatch = (requestPath.compare(0, candidateLocation.path.size(), candidateLocation.path) == 0);
        if (isPrefixMatch && candidateLocation.path.size() >= longestMatchLen)
        {
            bestLocationMatch = &candidateLocation;
            longestMatchLen = candidateLocation.path.size();
        }
    }

    if (!bestLocationMatch || (bestLocationMatch->path == "/" && requestPath != "/"))
    {
        std::string errorPageContent;
        if (readFile("./Error.html", errorPageContent))
        {
            HttpResponse response(404, "Not Found");
            response.setBody(errorPageContent, detectContentType("./Error.html"));
            return response;
        }
        return buildError(404, "Not Found", "File not found");
    }

    if (!bestLocationMatch->methods.empty())
    {
        bool isAllowed = false;
        for (size_t i = 0; i < bestLocationMatch->methods.size(); ++i)
        {
            if (toUpper(bestLocationMatch->methods[i]) == method)
            {
                isAllowed = true;
                break;
            }
        }
        if (!isAllowed)
            return buildError(405, "Method Not Allowed", "Method not allowed for route");
    }
    if (method == "GET")
        return handleGet(request, requestPath, config);

    HttpResponse response(200, "OK");
    response.setBody("Hello, World!", "text/plain");
    return response;
}

HttpResponse HttpHandler::handleGet(const HttpRequest& request, std::string requestPath, const ServerConfig& config)
{
    (void)request; 

    if (requestPath.empty() || requestPath == "/")
        requestPath = "/index.html";

    std::string fullPath = config.root + requestPath;
    std::string fileContent;

    bool isFound = readFile(fullPath, fileContent);

    if (!isFound && requestPath == "/index.html")
    {
        fullPath = "./index.html";
        isFound = readFile(fullPath, fileContent);
    }
    if (isFound)
    {
        HttpResponse response(200, "OK");
        response.setBody(fileContent, detectContentType(fullPath));
        return response;
    }
    return buildError(404, "Not Found", "File not found");
}

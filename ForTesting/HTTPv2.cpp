#include "../configParser/configParser.hpp"
#include <string>
#include <fstream>
#include <cctype>
#include "../Helpers.hpp"
#include "algorithm"


#include <string>
#include <map>
#include <sstream>


enum State 
{
	PARSE_REQUEST_LINE,
	PARSE_HEADERS,
	PARSE_BODY,
	PARSE_COMPLETE,
	PARSE_ERROR
};

class HttpRequest 
{

	private:
    std::string method;
    std::string target;
    std::string version;
    std::map<std::string, std::string> headers;
    std::string body;
    
    State state;
    size_t consumedBytes;
    int 	errorCode;

    void setError(int code);

	public:
    HttpRequest();
    ~HttpRequest();

    void reset();
    int parse(const std::string& rawBuffer);

    bool parseHeaders(std::istringstream& headerStream);
    bool parseRequestLine(std::istringstream& headerStream);
    int  extractBody(const std::string& payload, size_t& consumedBody);
    

    int getErrorCode() const;
    std::string getBody() const;
    std::string getMethod() const;
    std::string getTarget() const;
    std::string getVersion() const;
    size_t getConsumedBytes() const;
    std::string getHeader(const std::string& key) const;
    

    State getState() const { return state; }
};


class HttpResponse
{
	private:
    int statusCode;
    std::string reasonPhrase;
    std::map<std::string, std::string> headers;
    std::string body;

	public:
    HttpResponse();
    HttpResponse(int code, const std::string& reason);
    ~HttpResponse();

    void setHeader(const std::string& key, const std::string& value);
    void setBody(const std::string& content, const std::string& contentType);
    std::string toString() const;
};


class HttpHandler 
{
	private:
    const ServerConfig 	*Config;

    std::string 	contentType(const std::string& path);
    bool 			readFile(const std::string& filePath, std::string& content);
    std::string 	stripQuery(const std::string& path);
    const Location* matchLocation(const std::string& path);
    bool 			isMethodAllowed(const std::string& method, const Location& loc);
    HttpResponse 	handle404();
    HttpResponse 	buildError(int code, const std::string& reason, const std::string& detail);
	
	// METHODS
    HttpResponse 	GET(const HttpRequest& req, std::string route);
	public:
    HttpHandler(const ServerConfig &serverConfig);
    ~HttpHandler();

    HttpResponse process(const HttpRequest& req);
};





#include "HttpHandler.hpp"

HttpResponse::HttpResponse() : statusCode(200), reasonPhrase("OK")
{
    headers["Connection"] = "keep-alive";
}

HttpResponse::HttpResponse(int code, const std::string& reason) : statusCode(code), reasonPhrase(reason)
{
    headers["Connection"] = "keep-alive";
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


HttpRequest::HttpRequest() : state(PARSE_REQUEST_LINE), consumedBytes(0), errorCode(0) {}
HttpRequest::~HttpRequest() {}

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
            if (status == -1)
			{
                setError(400);
                return 1;
            }
            consumedBytes += bodyConsumed;
            state = PARSE_COMPLETE;
        }
    }
    return 1;
}

std::string HttpRequest::getHeader(const std::string& key) const
{
    std::map<std::string, std::string>::const_iterator it = headers.find(key);
    if (it != headers.end())
        return it->second;
    return "";
}

std::string HttpRequest::getBody()		const { return body; }
std::string HttpRequest::getMethod()	const { return method; }
std::string HttpRequest::getTarget() 	const { return target; }
std::string HttpRequest::getVersion()	const { return version; }
int HttpRequest::getErrorCode()			const { return errorCode; }
size_t HttpRequest::getConsumedBytes()	const { return consumedBytes; }

#include "HttpHandler.hpp"

HttpResponse HttpHandler::GET(const HttpRequest& request, std::string requestPath)
{
    (void)request; 

    if (requestPath.empty() || requestPath == "/")
        requestPath = "/index.html";

    std::string fullPath = Config->root + requestPath;
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
        response.setBody(fileContent, contentType(fullPath));
        return response;
    }
    return handle404();
}



#include "HttpHandler.hpp"

struct CompareLocations
{
    bool operator()(const Location& a, const Location& b) const
    {
        return a.path.size() > b.path.size();
    }
};


HttpHandler::HttpHandler(const ServerConfig &serverConfig) : Config(&serverConfig)
{
    std::sort(Config->Locations.begin(), Config->Locations.end(), CompareLocations());
}

HttpHandler::~HttpHandler() {}

std::string HttpHandler::contentType(const std::string& path)
{
    size_t extensionPos = path.find_last_of('.');
    if (extensionPos == std::string::npos) return "application/octet-stream";

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

std::string HttpHandler::stripQuery(const std::string& path)
{
    size_t queryStart = path.find('?');
    if (queryStart != std::string::npos)
        return path.substr(0, queryStart);
    return path;
}

const Location* HttpHandler::matchLocation(const std::string& path)
{
    for (size_t i = 0; i < Config->Locations.size(); ++i)
    {
        const Location& loc = Config->Locations[i];
        if (path.compare(0, loc.path.size(), loc.path) == 0)
        {
            return &loc;
        }
    }
    return NULL;
}

bool HttpHandler::isMethodAllowed(const std::string& method, const Location& loc)
{
    if (loc.methods.empty())
        return true; 
        
    for (size_t i = 0; i < loc.methods.size(); ++i)
    {
        if (toUpper(loc.methods[i]) == method)
            return true;
    }
    return false;
}

HttpResponse HttpHandler::handle404()
{
    std::string errorPageContent;
    std::string errorPath = Config->root + "/Error.html"; 
    
    if (readFile(errorPath, errorPageContent))
    {
        HttpResponse response(404, "Not Found");
        response.setBody(errorPageContent, contentType(errorPath));
        return response;
    }
    return buildError(404, "Not Found", "File not found");
}


HttpResponse HttpHandler::process(const HttpRequest& request)
{
    int errorCode = request.getErrorCode();
    if (errorCode != 0)
    {
        if (errorCode == 505) return buildError(505, "HTTP Version Not Supported", "Unsupported HTTP version");
        if (errorCode == 400) return buildError(400, "Bad Request", "Malformed request");
        return buildError(errorCode, "Error", "Parsing failed");
    }

    std::string method = request.getMethod();
    if (method != "GET" && method != "POST" && method != "DELETE")
        return buildError(405, "Method Not Allowed", "Unsupported method");

    std::string requestPath = stripQuery(request.getTarget());

    const Location* matchedLocation = matchLocation(requestPath);
    if (!matchedLocation)
        return handle404();

    if (!isMethodAllowed(method, *matchedLocation))
        return buildError(405, "Method Not Allowed", "Method not allowed for route");

    if (method == "GET")
        return GET(request, requestPath);

    HttpResponse response(200, "OK");
    response.setBody("Hello, World!", "text/plain");
    return response;
}


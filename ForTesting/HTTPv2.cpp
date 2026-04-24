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

namespace HttpUtils
{
    std::string contentType(const std::string& path);
    bool        readFile(const std::string& filePath, std::string& content);
    std::string stripQuery(const std::string& path);
}

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
    int parseRequestLine(const std::string& raw);
    int parseHeaders(const std::string& raw);
    int parseBody(const std::string& raw);
	public:
    HttpRequest();
    ~HttpRequest();

    void reset();
    int parse(const std::string& rawBuffer);

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
    std::vector<Location> sortedLocations;
    
    const Location* matchLocation(const std::string &path);
    bool 			isMethodAllowed(const std::string &method, const Location& loc);
    HttpResponse 	handle404();
    HttpResponse 	buildError(int code, const std::string& reason, const std::string& detail);
	
	// METHODS
    HttpResponse    CGI(const HttpRequest &request, const Location &matchedLocation, const std::string &requestPath);
    HttpResponse 	GET(const HttpRequest &request, std::string requestPath);
	void    DELETE(const HttpRequest &request, std::string requestPath);
    public:
    HttpHandler(const ServerConfig &serverConfig);
    ~HttpHandler();

    HttpResponse process(const HttpRequest& req);
};


namespace HttpUtils 
{

    std::string contentType(const std::string& path)
    {
        static std::map<std::string, std::string> mimeTypes;
        if (mimeTypes.empty())
        {
            mimeTypes.insert(std::make_pair(".html", "text/html"));
            mimeTypes.insert(std::make_pair(".htm", "text/html"));
            mimeTypes.insert(std::make_pair(".css", "text/css"));
            mimeTypes.insert(std::make_pair(".js", "application/javascript"));
            mimeTypes.insert(std::make_pair(".json", "application/json"));
            mimeTypes.insert(std::make_pair(".txt", "text/plain"));
            mimeTypes.insert(std::make_pair(".png", "image/png"));
            mimeTypes.insert(std::make_pair(".jpg", "image/jpeg"));
            mimeTypes.insert(std::make_pair(".jpeg", "image/jpeg"));
            mimeTypes.insert(std::make_pair(".gif", "image/gif"));
        }

        size_t pos = path.find_last_of('.');
        if (pos == std::string::npos) 
            return "application/octet-stream";
        std::string ext = toLower(path.substr(pos));
        std::map<std::string, std::string>::const_iterator it = mimeTypes.find(ext);
        if (it != mimeTypes.end()) 
            return it->second;
        return "application/octet-stream";
    }

    bool readFile(const std::string& filePath, std::string& content)
    {
        std::ifstream file(filePath.c_str(), std::ios::in | std::ios::binary);
        if (!file.is_open())
            return false;
        std::ostringstream buffer;
        buffer << file.rdbuf();
        content = buffer.str();
        return true;
    }

    std::string stripQuery(const std::string& path)
    {
        size_t pos = path.find('?');
        if (pos == std::string::npos) 
            return path;
        return path.substr(0, pos);
    }
}

#include "HttpHandler.hpp"

HttpRequest::HttpRequest() : state(PARSE_REQUEST_LINE), consumedBytes(0), errorCode(0) {}
HttpRequest::~HttpRequest() {}

std::string HttpRequest::getHeader(const std::string& key) const
{
    std::map<std::string, std::string>::const_iterator it = headers.find(key);
    if (it != headers.end())
        return it->second;
    return "";
}

void HttpRequest::setError(int code)
{
    errorCode = code;
    state = PARSE_ERROR;
}

std::string HttpRequest::getBody()		const { return body; }
std::string HttpRequest::getMethod()	const { return method; }
std::string HttpRequest::getTarget() 	const { return target; }
std::string HttpRequest::getVersion()	const { return version; }
int HttpRequest::getErrorCode()			const { return errorCode; }
size_t HttpRequest::getConsumedBytes()	const { return consumedBytes; }


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

int HttpRequest::parseRequestLine(const std::string& raw)
{
    size_t crlf = raw.find("\r\n", consumedBytes);
    if (crlf == std::string::npos) return 0; // Need more data

    std::string line = raw.substr(consumedBytes, crlf - consumedBytes);
    std::istringstream lineStream(line);
    
    if (!(lineStream >> method >> target >> version)) 
    {
        setError(400);
        return -1; 
    }
    
    method = toUpper(method);
    consumedBytes = crlf + 2;
    state = PARSE_HEADERS;
    return 1;
}

int HttpRequest::parseHeaders(const std::string& raw)
{
    size_t headerEnd = raw.find("\r\n\r\n", consumedBytes);
    if (headerEnd == std::string::npos) return 0; 

    std::string headerSection = raw.substr(consumedBytes, headerEnd - consumedBytes);
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
    
    consumedBytes = headerEnd + 4;
    state = PARSE_BODY;
    return 1; 
}

int HttpRequest::parseBody(const std::string& raw)
{
    std::string rawBodyData = raw.substr(consumedBytes);
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
        if (!parseChunkedBody(rawBodyData, body, bodyConsumed))
            return 0;
    }
    else if (cl_it != headers.end())
    {
        std::string lengthString = cl_it->second;
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
        if (rawBodyData.size() < contentLength)
            return 0;  
        body = rawBodyData.substr(0, contentLength);
        bodyConsumed = contentLength;
    }
    consumedBytes += bodyConsumed;
    state = PARSE_COMPLETE;
    return 1;
}

int HttpRequest::parse(const std::string &rawRequestData)
{
    while (state != PARSE_COMPLETE && state != PARSE_ERROR)
    {
        int status = 0;
        switch (state) 
        {
            case PARSE_REQUEST_LINE:
                status = parseRequestLine(rawRequestData); break;
            case PARSE_HEADERS:
                status = parseHeaders(rawRequestData); break;
            case PARSE_BODY:
                status = parseBody(rawRequestData); break;
            default: 
                return 1;
        }
        if (status <= 0) return status; // 0 = Need more data, -1 = Error
    }
    return 1;
}





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


#include "HttpHandler.hpp"

HttpResponse HttpHandler::GET(const HttpRequest& request, std::string requestPath)
{
    (void)request; 

    if (requestPath.empty() || requestPath == "/")
        requestPath = "/index.html";

    std::string fullPath = Config->root + requestPath;
    std::string fileContent;

    bool isFound = HttpUtils::readFile(fullPath, fileContent);

    if (!isFound && requestPath == "/index.html")
    {
        fullPath = "./index.html";
        isFound = HttpUtils::readFile(fullPath, fileContent);
    }
    if (isFound)
    {
        HttpResponse response(200, "OK");
        response.setBody(fileContent, HttpUtils::contentType(fullPath));
        return response;
    }
    return handle404();
}


void HttpHandler::DELETE(const HttpRequest &request, std::string requestPath)
{
    std::cout << requestPath << std::endl;
    (void)request;
}



#include "HttpHandler.hpp"
#include "../CGI/CGI.hpp"

struct CompareLocations
{
    bool operator()(const Location& a, const Location& b) const
    {
        return a.path.size() > b.path.size();
    }
};


HttpHandler::HttpHandler(const ServerConfig &serverConfig) : Config(&serverConfig)
{
    sortedLocations = Config->Locations;
    std::sort(sortedLocations.begin(), sortedLocations.end(), CompareLocations());
}

HttpHandler::~HttpHandler() {}

HttpResponse HttpHandler::buildError(int statusCode, const std::string& statusReason, const std::string& detail)
{
    HttpResponse response(statusCode, statusReason);
    response.setBody(detail, "text/plain");
    return response;
}


const Location* HttpHandler::matchLocation(const std::string& path)
{
    for (size_t i = 0; i < Config->Locations.size(); ++i)
    {
        const Location &loc = sortedLocations[i];
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
    
    if (HttpUtils::readFile(errorPath, errorPageContent))
    {
        HttpResponse response(404, "Not Found");
        response.setBody(errorPageContent, HttpUtils::contentType(errorPath));
        return response;
    }
    return buildError(404, "Not Found", "File not found");
}

HttpResponse HttpHandler::CGI(const HttpRequest &request, const Location &matchedLocation, const std::string &requestPath)
{
    CgiHandler cgi;
    std::string cgiOutput;
    try
    {
        cgiOutput = cgi.execute(request, matchedLocation, requestPath);
    }
    catch (const std::exception& e)
    {
        return buildError(500, "Internal Server Error", "CGI Execution Failed");
    }
    HttpResponse response(200, "OK");
    std::string contentType = "text/html";
    
    size_t delimiter = cgiOutput.find("\n\n");
    if (delimiter == std::string::npos)
        delimiter = cgiOutput.find("\r\n\r\n");

    if (delimiter == std::string::npos) {
        response.setBody(cgiOutput, contentType);
        return response;
    }
    std::string headersPart = cgiOutput.substr(0, delimiter);
    std::string bodyPart = cgiOutput.substr(delimiter + (cgiOutput[delimiter+1] == '\r' ? 4 : 2));

    std::stringstream ss(headersPart);
    std::string line;
    while (std::getline(ss, line)) {
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);

        if (line.find("Content-Type: ") == 0) {
            contentType = line.substr(14);
        }
        else if (line.find("Status: ") == 0) {
            // later
        }
    }
    response.setBody(bodyPart, contentType);
    return response;
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

    std::string requestPath = HttpUtils::stripQuery(request.getTarget());

    const Location* matchedLocation = matchLocation(requestPath);
    if (!matchedLocation)
        return handle404();

if (!matchedLocation->cgiExt.empty())
    {
        if (requestPath.size() >= matchedLocation->cgiExt.size() &&
            requestPath.compare(requestPath.size() - matchedLocation->cgiExt.size(), 
                                matchedLocation->cgiExt.size(), matchedLocation->cgiExt) == 0)
        {
            return CGI(request, *matchedLocation, requestPath); 
        }
    }
    if (!isMethodAllowed(method, *matchedLocation))
        return buildError(405, "Method Not Allowed", "Method not allowed for route");

    if (method == "GET")
        return GET(request, requestPath);
    if (method == "DELETE")
    {
        DELETE(request, requestPath);
        return GET(request, requestPath);
    }
    HttpResponse response(200, "OK");
    response.setBody("Hello, World!", "text/plain");
    return response;
}

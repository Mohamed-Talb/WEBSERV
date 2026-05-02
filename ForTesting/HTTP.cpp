#ifndef HTTPHANDLER_HPP
#define HTTPHANDLER_HPP

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

namespace FileSystem
{
    bool        fileExists(const std::string &filePath);
    bool        readFile(const std::string &filePath, std::string& content);
    bool        deleteFile(const std::string &filePath);
    bool        writeToFile(const std::string &filePath, std::string &content);
}

class HttpRequest 
{

	private:
    std::string method;
    std::string target;
    std::string version;
    std::map<std::string, std::string> headers;
    std::string body;
    
    State   state;
    size_t  parsedSize;
    int     errorCode;

    void setError(int code);
    int  parseBody(const std::string &raw);
    int  parseHeaders(const std::string &raw);
    int  parseRequestLine(const std::string &raw);
	public:
    HttpRequest();
    ~HttpRequest();

    void reset();
    int  parse(const std::string &rawBuffer);

    int getErrorCode() const;
    std::string getBody() const;
    size_t getParsedSize() const;
    std::string getMethod() const;
    std::string getTarget() const;
    std::string getVersion() const;
    std::string getHeader(const std::string &key) const;
    

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
    HttpResponse(int code, const std::string &reason);
    ~HttpResponse();

    void setHeader(const std::string &key, const std::string &value);
    void setBody(const std::string &content, const std::string &contentType);
    std::string toString() const;
};


class HttpHandler 
{
	private:
    const ServerConfig 	*serverConfig;
    
    const Location* matchLocation(const std::string &path);
    bool 			isMethodAllowed(const std::string &method, const Location& loc);
	// METHODS
    public:
	std::vector<std::string> resolveIndexFiles(const Location *loc);
	const Location *getCgiLocation(const HttpRequest &request);
    HttpHandler(const ServerConfig &serverConfig);
    ~HttpHandler();

    HttpResponse process(const HttpRequest& req);
};

namespace HttpUtils
{
    std::string contentType(const std::string &path);
    std::string stripQuery(const std::string &path);
    HttpResponse ErrorPage(int statusCode, const std::string &statusReason, const ServerConfig &config);
}


#include "Methods.hpp"
#include "HttpUtils.hpp"


HttpResponse HttpMethods::GET(const std::string &rootDirectory,
                              std::string requestPath,
                              const ServerConfig &config)
{
    std::string fullPath = joinPath(rootDirectory, requestPath);
    std::string fileContent;

    if (FileSystem::readFile(fullPath, fileContent))
    {
        HttpResponse response(200, "OK");
        response.setBody(fileContent, HttpUtils::contentType(fullPath));
        return response;
    }

    return HttpUtils::ErrorPage(404, "Not Found", config);
}


HttpResponse HttpMethods::DELETE(const std::string &rootDirectory, std::string requestPath, const ServerConfig &config)
{
    if (requestPath.find("..") != std::string::npos)
        return HttpUtils::ErrorPage(403, "Forbidden", config);
    std::string fullPath = rootDirectory + requestPath;
    if (!FileSystem::fileExists(fullPath)) 
    {
        return HttpUtils::ErrorPage(404, "Not Found", config);
    }
    if (!FileSystem::deleteFile(fullPath)) 
    {
        return HttpUtils::ErrorPage(403, "Forbidden", config);
    }
    return HttpResponse(204, "No Content");
}

// static void dbg_print(std::string identifier, std::string data)
// {
//     std::cout << identifier << data << std::endl;
// }

std::string getInBetween(std::string str, std::string s1, std::string s2)
{
    std::string result = str.substr(str.find(s1) + s1.size());
    result = result.substr(0, result.find(s2));
    return result;
}

void storeFile(std::string body, std::string rootDirectory)
{
    std::string disp = getInBetween(body, "Content-Disposition: ", "\r\n");

    std::cout << "trimedBody: " << body;
    // dbg_print("trimedBody: ", body);
    if (disp.find("filename=") != std::string::npos)
    {
        std::string filename = getInBetween(disp, "filename=\"", "\"");
        std::string content = body.substr(body.find("\r\n\r\n") + 4);
        FileSystem::writeToFile(rootDirectory + "/" + filename, content);
    }
}

HttpResponse HttpMethods::POST(const HttpRequest& request, const std::string &rootDirectory, std::string requestPath, const ServerConfig &config)
{
    (void) request;
    (void) rootDirectory;
    (void) requestPath;
    (void) config;
    
    std::string boundary = getInBetween(request.getHeader("content-type"), "boundary=", "\n");
    // dbg_print("boundary is: ", boundary);
    
    std::string trimedBody = getInBetween(request.getBody(), boundary, "--" + boundary);
    storeFile(trimedBody, rootDirectory);
    return HttpResponse(200, "OK");
}


#include "HttpHandler.hpp"

HttpRequest::HttpRequest() : state(PARSE_REQUEST_LINE), parsedSize(0), errorCode(0) {}
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
size_t HttpRequest::getParsedSize()	    const { return parsedSize; }


void HttpRequest::reset() 
{
    method.clear(); target.clear(); version.clear();
    headers.clear(); body.clear();
    parsedSize = 0; 
    errorCode = 0;
    state = PARSE_REQUEST_LINE;
}

static bool parseChunkedBody(const std::string &rawInputData, std::string& decodedBody, size_t& totalConsumed)
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
    std::string rawBodyData = raw.substr(parsedSize);
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
    parsedSize += bodyConsumed;
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


HttpHandler::HttpHandler(const ServerConfig &serverConfig) : serverConfig(&serverConfig) {}

HttpHandler::~HttpHandler() {}

const Location *HttpHandler::matchLocation(const std::string& path)
{
    for (size_t i = 0; i < serverConfig->Locations.size(); ++i)
    {
        const Location& loc = serverConfig->Locations[i];
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

const Location *HttpHandler::getCgiLocation(const HttpRequest& request)
{
    std::string requestPath = HttpUtils::stripQuery(request.getTarget());
    const Location *matchedLocation = matchLocation(requestPath);
    
    if (!matchedLocation) return NULL;

    if (!matchedLocation->cgiExt.empty())
    {
        if (requestPath.size() >= matchedLocation->cgiExt.size() &&
            requestPath.compare(requestPath.size() - matchedLocation->cgiExt.size(), 
                                matchedLocation->cgiExt.size(), matchedLocation->cgiExt) == 0)
        {
            return matchedLocation;
        }
    }
    return NULL;
}

std::vector<std::string> HttpHandler::resolveIndexFiles(const Location *loc)
{
    if (loc && !loc->indexes.empty())
        return loc->indexes;
    if (!serverConfig->indexes.empty())
        return serverConfig->indexes;
    std::vector<std::string> defaults;
    defaults.push_back("index.html");
    return defaults;
}

HttpResponse HttpHandler::process(const HttpRequest &request)
{
    if (request.getErrorCode() != 0)
        return HttpUtils::ErrorPage(request.getErrorCode(), "Bad Request", *serverConfig);

    std::string method = request.getMethod();
    std::string requestPath = HttpUtils::stripQuery(request.getTarget());

    const Location *matchedLocation = matchLocation(requestPath);
    if (!matchedLocation)
        return HttpUtils::ErrorPage(404, "Not Found", *serverConfig);

    if (!isMethodAllowed(method, *matchedLocation))
        return HttpUtils::ErrorPage(405, "Method Not Allowed", *serverConfig);

    std::string root = matchedLocation->root.empty() ? serverConfig->root : matchedLocation->root;
    std::string fullPath = joinPath(root, requestPath);

    struct stat S;
    if (stat(fullPath.c_str(), &S) == 0 && S_ISDIR(S.st_mode))
    {
        std::vector<std::string> indexes = resolveIndexFiles(matchedLocation);
        bool foundIndex = false;
        for (size_t i = 0; i < indexes.size(); ++i)
        {
            std::string candidatePath = joinPath(fullPath, indexes[i]);
            if (FileSystem::fileExists(candidatePath))
            {
                requestPath = joinPath(requestPath, indexes[i]);
                foundIndex = true;
                break;
            }
        }
        if (!foundIndex)
        {
            return HttpUtils::ErrorPage(403, "Forbidden", *serverConfig);
        }
    }
    if (method == "GET")
        return HttpMethods::GET(root, requestPath, *serverConfig);
    if (method == "DELETE")
        return HttpMethods::DELETE(root, requestPath, *serverConfig);
    return HttpUtils::ErrorPage(501, "Not Implemented", *serverConfig);
}


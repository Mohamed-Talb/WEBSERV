
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


#include "HttpHandler.hpp"
namespace HttpUtils
{
    std::string contentType(const std::string &path);
    std::string stripQuery(const std::string &path);
    HttpResponse ErrorPage(int statusCode, const std::string &statusReason, const ServerConfig &config);
}


#include "HttpHandler.hpp"
#include <string>

class HttpMethods 
{
    public:
    static HttpResponse GET(const std::string &rootDirectory, std::string requestPath, const ServerConfig &config);
    static HttpResponse DELETE(const std::string &rootDirectory, std::string requestPath, const ServerConfig &config);

    // static HttpResponse DELETE(const std::string& rootDirectory, const std::string& targetPath);
    // static HttpResponse POST(const std::string& rootDirectory, const HttpRequest& request); 
};

#include "HttpHandler.hpp" // Assuming this contains your FileSystem declaration
#include <iostream>
#include <fstream>
#include <cstdio>
#include <unistd.h>
#include <sys/stat.h>

namespace FileSystem
{
    bool fileExists(const std::string &filePath)
    {
        return (access(filePath.c_str(), F_OK) == 0);
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

    bool deleteFile(const std::string &filePath)
    {
        if (std::remove(filePath.c_str()) == 0)
            return true;
        return false;
    }

    bool writeToFile(const std::string &filePath, std::string &content)
    {
        std::ofstream outfile(filePath.c_str(), std::ios::out | std::ios::trunc);
        if (!outfile.is_open())
            return false;
        
        outfile << content;
        outfile.close();
        return true;
    }
}



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

    std::string stripQuery(const std::string& path)
    {
        size_t pos = path.find('?');
        if (pos == std::string::npos) 
            return path;
        return path.substr(0, pos);
    }
    HttpResponse ErrorPage(int statusCode, const std::string &statusReason, const ServerConfig &config)
    {
        std::string errorPageContent;
        std::string errorPath;
        std::map<int, std::string>::const_iterator it = config.errorPage.find(statusCode);
        
        if (it != config.errorPage.end()) {
            errorPath = config.root + it->second;
        }
        if (!errorPath.empty() && FileSystem::readFile(errorPath, errorPageContent))
        {
            HttpResponse response(statusCode, statusReason);
            response.setBody(errorPageContent, contentType(errorPath));
            return response;
        }
        std::ostringstream defaultHtml;
        defaultHtml << "<html><head><title>" << statusCode << " " << statusReason << "</title></head>"
                    << "<body><center><h1>" << statusCode << " " << statusReason << "</h1></center>"
                    << "<hr><center>webserv/1.0</center></body></html>";

        HttpResponse response(statusCode, statusReason);
        response.setBody(defaultHtml.str(), "text/html");
        return response;
    }
}

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



#include "configParser.hpp"

std::string expect(TokenIt &it, const Tokens &tokens, const std::string &err)
{
    if (it == tokens.end())
        throw std::runtime_error(err);
    return *it++;
}

void expectSemicolon(TokenIt &it, const Tokens &tokens, const std::string &directive)
{
    if (expect(it, tokens, "Missing ';' after " + directive) != ";")
        throw std::runtime_error("Expected ';' after " + directive);
}

int parsePort(TokenIt &it, const Tokens &tokens)
{
    std::string value = expect(it, tokens, "Missing port");
	if (value.size() > 5)
        throw std::runtime_error("Invalid port: " + value);
    int port;
    try
    {
        port = std::atoi(value.c_str());
    }
    catch (...)
    {
        throw std::runtime_error("Invalid port: " + value);
    }
    if (0 < port || port > 65535)
        throw std::runtime_error("Invalid port: " + value);
    return static_cast<int>(port);
}

size_t parseBodySize(TokenIt &it, const Tokens &tokens)
{
    std::string value = expect(it, tokens, "Missing body size value");
    try
    {
        return myStold(value);
    }
    catch (...)
    {
        throw std::runtime_error("Invalid client_max_body_size value: " + value);
    }
}

std::string parseRoot(TokenIt &it, const Tokens &tokens)
{
    std::string root = expect(it, tokens, "Missing root");

    root = mergeSlashes(root);

    while (root.size() > 1 && root[root.size() - 1] == '/')
        root.erase(root.size() - 1);

    if (root.empty())
        throw std::runtime_error("Invalid root path");

    if (root.find("..") != std::string::npos)
        throw std::runtime_error("Invalid root path: directory traversal");

    return root;
}

std::string parseLocationPath(TokenIt &it, const Tokens &tokens)
{
    std::string path = expect(it, tokens, "Missing location path");

    path = mergeSlashes(path);

    if (path.empty() || path[0] != '/')
        throw std::runtime_error("Location path must start with '/'");

    while (path.size() > 1 && path[path.size() - 1] == '/')
        path.erase(path.size() - 1);

    if (path.find("..") != std::string::npos)
        throw std::runtime_error("Invalid location path: directory traversal");

    return path;
}

std::vector<std::string> parseIndexes(TokenIt &it, const Tokens &tokens)
{
    std::vector<std::string> indexes;
    while (it != tokens.end() && *it != ";")
    {
        std::string index = expect(it, tokens, "Missing index value");

        index = mergeSlashes(index);

        while (!index.empty() && index[0] == '/')
            index.erase(0, 1);

        if (index.empty())
            throw std::runtime_error("Index cannot be empty");
		
        if (index.find("..") != std::string::npos)
            throw std::runtime_error("Invalid index path: directory traversal");

        indexes.push_back(index);
    }
    if (it == tokens.end())
        throw std::runtime_error("Missing ';' after index");

    if (indexes.empty())
        throw std::runtime_error("index directive requires at least one file");
    ++it;
    return indexes;
}

std::string parseCgiExt(TokenIt &it, const Tokens &tokens)
{
    std::string ext = expect(it, tokens, "Missing cgi_ext");

    if (ext.empty())
        throw std::runtime_error("Invalid cgi_ext");

    if (ext[0] != '.')
        ext = "." + ext;

    return ext;
}

std::string parseErrorPagePath(const std::string& raw)
{
    std::string path = mergeSlashes(raw);

    if (path.empty())
        throw std::runtime_error("Invalid error_page path");

    if (path.find("..") != std::string::npos)
        throw std::runtime_error("Invalid error_page path");

    if (path[0] != '/')
        path = "/" + path;

    return path;
}



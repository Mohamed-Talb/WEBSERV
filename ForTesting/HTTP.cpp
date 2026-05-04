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

#include "HttpHandler.hpp"
#include <fstream>
#include <sstream>
#include <map>
#include "../Helpers.hpp"
#include <sys/stat.h>

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
            response.setBody(errorPageContent);
            response.setHeader("Content-Type", contentType(errorPath));
            return response;
        }
        std::ostringstream defaultHtml;
        defaultHtml << "<html><head><title>" << statusCode << " " << statusReason << "</title></head>"
                    << "<body><center><h1>" << statusCode << " " << statusReason << "</h1></center>"
                    << "<hr><center>webserv/1.0</center></body></html>";

        HttpResponse response(statusCode, statusReason);
        response.setBody(defaultHtml.str());
        response.setHeader("Content-Type","text/html");
        return response;
    }

    bool isDirectory(const std::string &path)
    {
        struct stat s;
        if (stat(path.c_str(), &s) != 0)
            return false;
        return S_ISDIR(s.st_mode);
    }
}

HttpResponse HttpMethods::GET(RouteMatch* match, const ServerConfig& config)
{
    if (!match)
        return HttpUtils::ErrorPage(500, "Internal Server Error", config);

    std::string fileContent;

    if (FileSystem::readFile(match->fullPath, fileContent))
    {
        HttpResponse response(200, "OK");
        response.setBody(fileContent);
        response.setHeader("Content-Type", HttpUtils::contentType(match->fullPath));
        return response;
    }

    return HttpUtils::ErrorPage(404, "Not Found", config);
}


HttpResponse HttpMethods::DELETE(RouteMatch* match, const ServerConfig& config)
{
    if (!match)
        return HttpUtils::ErrorPage(500, "Internal Server Error", config);

    if (match->requestPath.find("..") != std::string::npos)
        return HttpUtils::ErrorPage(403, "Forbidden", config);

    if (!FileSystem::fileExists(match->fullPath))
        return HttpUtils::ErrorPage(404, "Not Found", config);

    if (!FileSystem::deleteFile(match->fullPath))
        return HttpUtils::ErrorPage(403, "Forbidden", config);

    return HttpResponse(204, "No Content");
}


#include "Methods.hpp"
#include "HttpUtils.hpp"
#include "HttpHandler.hpp"

/*
--BOUNDARY\r\n
Content-Disposition: form-data; name="file"; filename="cat.png"\r\n
Content-Type: image/png\r\n
\r\n
FILE_CONTENT_HERE\r\n
--BOUNDARY--\r\n
*/

static bool isSafeFilename(const std::string &filename)
{
    if (filename.empty())
        return false;

    if (filename.find("..") != std::string::npos)
        return false;

    if (filename.find('/') != std::string::npos)
        return false;

    if (filename.find('\\') != std::string::npos)
        return false;

    return true;
}

static bool extractBoundary(const std::string &contentType, std::string &boundary)
{
    std::string key = "boundary=";
    size_t pos = contentType.find(key);

    if (pos == std::string::npos)
        return false;

    boundary = contentType.substr(pos + key.size());

    size_t semicolon = boundary.find(';');
    if (semicolon != std::string::npos)
        boundary = boundary.substr(0, semicolon);

    boundary = trim(boundary);

    if (boundary.size() >= 2 && boundary[0] == '"' && boundary[boundary.size() - 1] == '"')
        boundary = boundary.substr(1, boundary.size() - 2);

    return !boundary.empty();
}

static bool extractFilename(const std::string &partHeaders, std::string &filename)
{
    std::string key = "filename=\"";
    size_t pos = partHeaders.find(key);

    if (pos == std::string::npos)
        return false;

    pos += key.size();

    size_t end = partHeaders.find("\"", pos);
    if (end == std::string::npos)
        return false;

    filename = partHeaders.substr(pos, end - pos);
    return isSafeFilename(filename);
}

static bool parseMultipartFile(const std::string &body, const std::string &boundary, std::string &filename, std::string &fileContent)
{
    std::string delimiter = "--" + boundary;
    std::string closingDelimiter = "--" + boundary + "--";

    size_t partStart = body.find(delimiter);
    if (partStart == std::string::npos)
        return false;

    partStart += delimiter.size();

    if (body.compare(partStart, 2, "\r\n") != 0)
        return false;

    partStart += 2;

    size_t headersEnd = body.find("\r\n\r\n", partStart);
    if (headersEnd == std::string::npos)
        return false;

    std::string partHeaders = body.substr(partStart, headersEnd - partStart);

    if (!extractFilename(partHeaders, filename))
        return false;

    size_t contentStart = headersEnd + 4;

    size_t nextBoundary = body.find("\r\n" + delimiter, contentStart);
    if (nextBoundary == std::string::npos)
        return false;

    fileContent = body.substr(contentStart, nextBoundary - contentStart);

    return true;
}

HttpResponse HttpMethods::POST(const HttpRequest &request, RouteMatch *match, const ServerConfig &config)
{
    if (!match || !match->location)
        return HttpUtils::ErrorPage(500, "Internal Server Error!", config);

    if (match->location->uploadEnabled != "on")
        return HttpUtils::ErrorPage(403, "Forbidden", config);

    if (match->location->uploadPath.empty())
        return HttpUtils::ErrorPage(500, "Internal Server Error2", config);

    if (!HttpUtils::isDirectory(match->location->uploadPath))
        return HttpUtils::ErrorPage(500, match->location->uploadPath, config);

    std::string contentType = request.getHeader("content-type");

    if (contentType.find("multipart/form-data") == std::string::npos)
        return HttpUtils::ErrorPage(415, "Unsupported Media Type", config);

    std::string boundary;
    if (!extractBoundary(contentType, boundary))
        return HttpUtils::ErrorPage(400, "Bad Request", config);

    std::string filename;
    std::string fileContent;

    if (!parseMultipartFile(request.getBody(), boundary, filename, fileContent))
        return HttpUtils::ErrorPage(400, "Bad Request", config);

    std::string outputPath = joinPath(match->location->uploadPath, filename);

    if (!FileSystem::writeToFile(outputPath, fileContent))
        return HttpUtils::ErrorPage(500, "Internal Server Error4", config);

    HttpResponse response(201, "Created");
    response.setBody("File uploaded successfully\n");
    response.setHeader("Content-Type", "text/plain");
    return response;
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

void HttpResponse::setBody(const std::string &content)
{
    body = content;

    std::ostringstream sizeStream;
    sizeStream << body.size();
    headers["Content-Length"] = sizeStream.str();
}

void HttpResponse::writeBody(const std::string &chunk)
{
    body += chunk;

    std::ostringstream sizeStream;
    sizeStream << body.size();
    headers["Content-Length"] = sizeStream.str();
}

bool HttpResponse::setBodyFromFile(const std::string &filePath)
{
    std::ifstream file(filePath.c_str(), std::ios::in | std::ios::binary);

    if (!file.is_open())
        return false;

    std::ostringstream buffer;
    buffer << file.rdbuf();

    if (file.bad())
        return false;

    body = buffer.str();

    std::ostringstream sizeStream;
    sizeStream << body.size();
    headers["Content-Length"] = sizeStream.str();

    return true;
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
#include "../CGI/CGI.hpp"
#include "Methods.hpp" 
#include "HttpUtils.hpp"
#include <sys/stat.h>
#include <dirent.h>

HttpHandler::HttpHandler(const ServerConfig &serverConfig) : serverConfig(&serverConfig) {}

HttpHandler::~HttpHandler() {}

const Location* HttpHandler::matchLocation(const std::string &path)
{
    const Location* bestMatch = NULL;
    size_t bestLength = 0;

    for (size_t i = 0; i < serverConfig->Locations.size(); ++i)
    {
        const Location& loc = serverConfig->Locations[i];
        if (path.compare(0, loc.path.size(), loc.path) == 0)
        {
            if (loc.path == "/" || path.size() == loc.path.size() || path[loc.path.size()] == '/')
            {
                if (loc.path.size() > bestLength)
                {
                    bestMatch = &loc;
                    bestLength = loc.path.size();
                }
            }
        }
    }
    return bestMatch;
}

bool HttpHandler::isMethodAllowed(const std::string &method, const Location &loc)
{
    std::cout << loc.path << std::endl;   
    std::cout << loc.root << std::endl;   
    if (loc.methods.empty())
        return true;
    for (size_t i = 0; i < loc.methods.size(); ++i)
    {
        std::cout << loc.methods[i] << std::endl;
        if (toUpper(loc.methods[i]) == method)
            return true;
    }
    return false;
}

const Location *HttpHandler::getCgiLocation(const HttpRequest &request)
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

void HttpHandler::resolveRoute(const HttpRequest& request, RouteMatch& match)
{
    match.location = NULL;
    match.requestPath.clear();
    match.root.clear();
    match.fullPath.clear();

    std::string requestPath = HttpUtils::stripQuery(request.getTarget());

    const Location* location = matchLocation(requestPath);
    if (!location)
        return;

    std::string root = location->root.empty() ? serverConfig->root : location->root;
    std::string relativePath = requestPath;
    if (location->path != "/")
    {
        relativePath = requestPath.substr(location->path.size());
        if (relativePath.empty())
            relativePath = "/";
    }
    std::string fullPath = joinPath(root, relativePath);
    std::cout << match.location << std::endl;
    std::cout << match.requestPath << std::endl;
    std::cout << match.root << std::endl;
    std::cout << match.fullPath << std::endl;
}

HttpResponse HttpHandler::process(const HttpRequest& request)
{
    if (request.getErrorCode() != 0)
        return HttpUtils::ErrorPage(request.getErrorCode(), "Bad Request", *serverConfig);

    RouteMatch match;
    resolveRoute(request, match);
    if (!match.location)
        return HttpUtils::ErrorPage(404, "Not Found", *serverConfig);

    if (match.location->redirectCode != 0)
    {
        int code = match.location->redirectCode;
        std::string reason = code == 301 ? "Moved Permanently" : "Found";
        HttpResponse res(code, reason);
        res.setHeader("Location", match.location->redirectTarget);
        res.setHeader("Content-Length", "0");
        return res;
    }
    std::string method = request.getMethod();
    if (!isMethodAllowed(method, *match.location))
        return HttpUtils::ErrorPage(405, "Method Not Allowed", *serverConfig);

    if (HttpUtils::isDirectory(match.fullPath))
    {
        std::vector<std::string> indexes = resolveIndexFiles(match.location);
        bool foundIndex = false;
        for (size_t i = 0; i < indexes.size(); ++i)
        {
            std::string candidatePath = joinPath(match.fullPath, indexes[i]);
            if (FileSystem::fileExists(candidatePath))
            {
                match.requestPath = joinPath(match.requestPath, indexes[i]);
                match.fullPath = candidatePath;
                foundIndex = true;
                break;
            }
        }
        if (!foundIndex)
        {
            if (method == "GET" && match.location->autoindex == "on")
                return generateDirectoryListing(match, serverConfig);
            return HttpUtils::ErrorPage(403, "Forbidden", *serverConfig);
        }
    }

    if (method == "GET")
        return HttpMethods::GET(&match, *serverConfig);

    if (method == "DELETE")
        return HttpMethods::DELETE(&match, *serverConfig);

    if (method == "POST")
        return HttpMethods::POST(request, &match, *serverConfig);

    return HttpUtils::ErrorPage(501, "Not Implemented", *serverConfig);
}
#include "../HttpHandler.hpp"
#include "HttpUtils.hpp"
#include <sys/stat.h>
#include <dirent.h>
#include <sstream>
#include <iomanip>
#include <cctype>


static std::string urlEncode(const std::string &str)
{
    std::ostringstream out;
    for (size_t i = 0; i < str.size(); ++i)
    {
        unsigned char c = static_cast<unsigned char>(str[i]);
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
        {
            out << c;
        }
        else
        {
            out << '%' << std::uppercase << std::hex << std::setw(2)
                << std::setfill('0') << static_cast<int>(c) << std::nouppercase << std::dec;
        }
    }
    return out.str();
}

static std::string htmlEscaping(std::string &srcHtml)
{
    std::string newHtml;
    char currChar;
    for(size_t i = 0; i < srcHtml.length(); i++)
    {
        currChar = srcHtml[i];
        if(currChar == '&')
            newHtml += "&amp;";
        else if(currChar == '<')
            newHtml += "&lt;";
        else if(currChar == '>')
            newHtml += "&gt;";
        else if(currChar == '"')
            newHtml += "&quot;";
        else
            newHtml += currChar;
    }
    return newHtml;
}

HttpResponse resolveAutoIndexing(const RouteMatch &match, const ServerConfig &serverConfig)
{
    DIR *dir = opendir(match.fullPath.c_str());

    if (dir == NULL)
        return ErrorPage(403, "Forbidden", serverConfig);

    HttpResponse response(200, "OK");
    response.setHeader("Content-Type", "text/html");

    std::string urlBase = match.requestPath;

    if (urlBase.empty())
        urlBase = "/";

    if (urlBase[urlBase.size() - 1] != '/')
        urlBase += "/";

    response.writeBody("<html>");
    response.writeBody("<head><title>Index of ");
    response.writeBody(htmlEscaping(urlBase));
    response.writeBody("</title></head>");
    response.writeBody("<body>");
    response.writeBody("<h1>Index of ");
    response.writeBody(htmlEscaping(urlBase));
    response.writeBody("</h1>");
    response.writeBody("<ul>");

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        std::string entryName = entry->d_name;
        if (entryName == "." || entryName == "..")
            continue;
        std::string entryFullPath = joinPath(match.fullPath, entryName);
        bool isDir = isDirectory(entryFullPath);
        std::string hrefName = urlEncode(entryName);
        std::string displayName = htmlEscaping(entryName);
        if (isDir)
        {
            hrefName += "/";
            displayName += "/";
        }
        response.writeBody("<li><a href=\"");
        response.writeBody(urlBase + hrefName);
        response.writeBody("\">");
        response.writeBody(displayName);
        response.writeBody("</a></li>");
    }
    closedir(dir);
    response.writeBody("</ul>");
    response.writeBody("</body>");
    response.writeBody("</html>");
    return response;
}


#include "HttpUtils.hpp"

HttpResponse ErrorPage(int statusCode, const std::string &statusReason, const ServerConfig &config)
{
    std::string errorPageContent;
    std::string errorPath;
    std::map<int, std::string>::const_iterator it = config.errorPage.find(statusCode);
    
    if (it != config.errorPage.end()) 
    {
        errorPath = config.root + it->second;
    }
    if (!errorPath.empty() && readFile(errorPath, errorPageContent))
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


#include "HttpUtils.hpp"

std::string contentType(const std::string &path)
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

#include "Methods.hpp"

HttpResponse HttpMethods::DELETE(const RouteMatch &match, const ServerConfig& config)
{
    if (match.requestPath.find("..") != std::string::npos)
        return ErrorPage(403, "Forbidden", config);

    if (!fileExists(match.fullPath))
        return ErrorPage(404, "Not Found", config);

    if (!deleteFile(match.fullPath))
        return ErrorPage(403, "Forbidden", config);

    return HttpResponse(204, "No Content");
}

#include "Methods.hpp"

HttpResponse HttpMethods::GET(const RouteMatch &match, const ServerConfig &config)
{
    std::string fileContent;

    if (readFile(match.fullPath, fileContent))
    {
        HttpResponse response(200, "OK");
        response.setBody(fileContent);
        response.setHeader("Content-Type", contentType(match.fullPath));
        return response;
    }

    return ErrorPage(404, "Not Found", config);
}


#include "Methods.hpp"

/*
--BOUNDARY\r\n
Content-Disposition: form-data; name="file"; filename="cat.png"\r\n
Content-Type: image/png\r\n
\r\n
FILE_CONTENT_HERE\r\n
--BOUNDARY--\r\n
*/

struct MultipartFileInfo
{
    std::string filename;
    size_t contentStart;
    size_t contentLength;

    MultipartFileInfo() : contentStart(0), contentLength(0) {}
};

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

static bool parseMultipartFileInfo(const std::string &body, const std::string &boundary, MultipartFileInfo &info)
{
    std::string delimiter = "--" + boundary;

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

    if (!extractFilename(partHeaders, info.filename))
        return false;

    info.contentStart = headersEnd + 4;

    size_t nextBoundary = body.find("\r\n" + delimiter, info.contentStart);
    if (nextBoundary == std::string::npos)
        return false;

    info.contentLength = nextBoundary - info.contentStart;
    return true;
}

static bool writeBufferToFile(const std::string &filePath, const char *data, size_t size)
{
    std::ofstream outfile(filePath.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);

    if (!outfile.is_open())
        return false;

    outfile.write(data, size);
    if (!outfile.good())
        return false;

    outfile.close();
    return true;
}

HttpResponse HttpMethods::POST(const HttpRequest &request, const RouteMatch &match, const ServerConfig &config)
{
    if (!match.location)
        return ErrorPage(500, "Internal Server Error", config);

    if (!match.location || match.location->uploadEnabled != "on")
        return ErrorPage(403, "Forbidden", config);

    if (match.location->uploadPath.empty() || !isDirectory(match.location->uploadPath))
        return ErrorPage(500, "Internal Server Error", config);

    std::string contentType = request.getHeader("content-type");
    std::string outputPath;
    const char* dataStart = NULL;
    size_t dataLength = 0;

    if (contentType.find("multipart/form-data") != std::string::npos)
    {
        std::string boundary;
        if (!extractBoundary(contentType, boundary))
            return ErrorPage(400, "Bad Request", config);

        MultipartFileInfo fileInfo;
        if (!parseMultipartFileInfo(request.getBody(), boundary, fileInfo))
            return ErrorPage(400, "Bad Request", config);

        outputPath = joinPath(match.location->uploadPath, fileInfo.filename);
        dataStart = request.getBody().data() + fileInfo.contentStart;
        dataLength = fileInfo.contentLength;
    }
    else 
    {
        outputPath = joinPath(match.location->uploadPath, "upload.bin");
        dataStart = request.getBody().data();
        dataLength = request.getBody().size();
    }

    if (!dataStart || (dataStart + dataLength > request.getBody().data() + request.getBody().size()))
        return ErrorPage(400, "Bad Request", config);

    if (!writeBufferToFile(outputPath, dataStart, dataLength))
        return ErrorPage(500, "Internal Server Error", config);

    HttpResponse response(201, "Created");
    response.setBody("File uploaded successfully to: " + outputPath + "\n");
    response.setHeader("Content-Type", "text/plain");

    return response;
}

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



#include "HttpResponse.hpp"

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


HttpHandler::HttpHandler(const ServerConfig &serverConfig) : serverConfig(&serverConfig) {}

HttpHandler::~HttpHandler() {}

const Location* HttpHandler::matchLocation(const std::string &path)
{
    const Location* bestMatch = NULL;
    size_t bestLength = 0;
    for (size_t i = 0; i < serverConfig->Locations.size(); ++i)
    {
        const Location &loc = serverConfig->Locations[i];
        if (path.size() >= loc.path.size() && path.compare(0, loc.path.size(), loc.path) == 0)
        {
            if (loc.path == "/" || 
                path.size() == loc.path.size() || 
                loc.path[loc.path.size() - 1] == '/' || 
                path[loc.path.size()] == '/')
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
    std::string requestPath = request.getRequestPath();
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

void HttpHandler::resolveRoute(const HttpRequest &request, RouteMatch& match)
{
    match.location = NULL;
    match.requestPath = request.getRequestPath();
    
    const Location *location = matchLocation(match.requestPath);
    if (location)
    {
        match.location = location;
        match.root = location->root.empty() ? serverConfig->root : location->root;

        std::string relativePath = match.requestPath;
        if (location->path != "/")
        {
            relativePath = match.requestPath.substr(location->path.size());
            if (relativePath.empty() || relativePath[0] != '/')
            {
                relativePath = "/" + relativePath;
            }
        }
        match.fullPath = joinPath(match.root, relativePath);
    }
    else
    {
        match.root = serverConfig->root;
        match.fullPath = joinPath(match.root, match.requestPath);
    }
}

HttpResponse resolveRedirection(const RouteMatch &match)
{
    int code = match.location->redirectCode;

    std::string reason;
    switch (code)
    {
        case 301: reason = "Moved Permanently"; break;
        case 302: reason = "Found"; break;
        case 303: reason = "See Other"; break;
        case 307: reason = "Temporary Redirect"; break;
        case 308: reason = "Permanent Redirect"; break;
        default:
            HttpResponse error(500, "Internal Server Error");
            error.setHeader("Content-Length", "0");
            return error;
    }
    if (match.location->redirectTarget.empty())
    {
        HttpResponse error(500, "Internal Server Error");
        error.setHeader("Content-Length", "0");
        return error;
    }

    HttpResponse response(code, reason);
    response.setHeader("Location", match.location->redirectTarget);
    response.setHeader("Content-Length", "0");

    return response;
}


HttpResult HttpHandler::process(const HttpRequest& request)
{
    if (request.getErrorCode() != 0)
    {
        return HttpResult::makeResponse(ErrorPage(request.getErrorCode(), "Bad Request", *serverConfig));
    }

    RouteMatch match;
    resolveRoute(request, match);
    if (match.location && match.location->redirectCode != 0)
    {
        return HttpResult::makeResponse(resolveRedirection(match));
    }
    std::string method = request.getMethod();
    bool allowed = match.location ? isMethodAllowed(method, *match.location) : (method == "GET");
    
    if (!allowed)
    {
        return HttpResult::makeResponse(ErrorPage(405, "Method Not Allowed", *serverConfig));
    }

    if (request.getBody().size() > static_cast<size_t>(serverConfig->client_max_body_size))
    {
        return HttpResult::makeResponse(ErrorPage(413, "Payload Too Large", *serverConfig));
    }

    if (isDirectory(match.fullPath))
    {
        std::vector<std::string> indexes = resolveIndexFiles(match.location);
        bool foundIndex = false;
        for (size_t i = 0; i < indexes.size(); ++i)
        {
            std::string candidatePath = joinPath(match.fullPath, indexes[i]);
            if (fileExists(candidatePath))
            {
                match.requestPath = joinPath(match.requestPath, indexes[i]);
                match.fullPath = candidatePath;
                foundIndex = true;
                break;
            }
        }

        if (!foundIndex)
        {
            bool autoIndexOn = (match.location && match.location->autoindex == "on");
            if (method == "GET" && autoIndexOn)
            {
                return HttpResult::makeResponse(resolveAutoIndexing(match, *serverConfig));
            }
            return HttpResult::makeResponse(ErrorPage(403, "Forbidden", *serverConfig));
        }
    }

    if (!fileExists(match.fullPath) && method != "POST")
    {
        return HttpResult::makeResponse(ErrorPage(404, "Not Found", *serverConfig));
    }

    if (match.location && !match.location->cgiExt.empty())
    {
        if (match.fullPath.size() >= match.location->cgiExt.size() &&
            match.fullPath.compare(match.fullPath.size() - match.location->cgiExt.size(), 
                                   match.location->cgiExt.size(), match.location->cgiExt) == 0)
        {
            return HttpResult::makeCgi(match.location, match.fullPath);
        }
    }

    HttpResponse response;
    if (method == "GET")
    {
        response = HttpMethods::GET(match, *serverConfig);
    }
    else if (method == "DELETE")
    {
        response = HttpMethods::DELETE(match, *serverConfig);
    }
    else if (method == "POST")
    {
        response = HttpMethods::POST(request, match, *serverConfig);
    }
    else 
    {
        response = ErrorPage(501, "Not Implemented", *serverConfig);
    }

    return HttpResult::makeResponse(response);
}


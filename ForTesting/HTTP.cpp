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


#include "HttpRequest.hpp"
#include "../Helpers.hpp"


HttpRequest::HttpRequest() : state(PARSE_REQUEST_LINE), parsedSize(0), errorCode(0) {}
HttpRequest::~HttpRequest() {}

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
    match.requestPath.clear();
    match.root.clear();
    match.fullPath.clear();

    std::string requestPath = request.getRequestPath();

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
    match.location = location;
    match.requestPath = requestPath;
    match.root = root;
    match.fullPath = fullPath;
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
        return HttpResult::makeResponse(ErrorPage(request.getErrorCode(), "Bad Request", *serverConfig));

    RouteMatch match;
    resolveRoute(request, match);
    if (!match.location)
        return HttpResult::makeResponse(ErrorPage(404, "Not Found", *serverConfig));

    if (match.location->redirectCode != 0)
    {
        return HttpResult::makeResponse(resolveRedirection(match));
    }
    std::string method = request.getMethod();
    if (!isMethodAllowed(method, *match.location))
        return HttpResult::makeResponse(ErrorPage(405, "Method Not Allowed", *serverConfig));
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
            if (method == "GET" && match.location->autoindex == "on")
                return HttpResult::makeResponse(resolveAutoIndexing(match, *serverConfig));
            return HttpResult::makeResponse(ErrorPage(403, "Forbidden", *serverConfig));
        }
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
        response = HttpMethods::GET(match, *serverConfig);
    else if (method == "DELETE")
       response = HttpMethods::DELETE(match, *serverConfig);
    else if (method == "POST")
        response = HttpMethods::POST(request, match, *serverConfig);
    else 
        response = ErrorPage(501, "Not Implemented", *serverConfig);
    return HttpResult::makeResponse(response);
}

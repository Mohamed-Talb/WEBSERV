#include "HttpRequest.hpp"
#include "../Helpers.hpp"

HttpRequest::HttpRequest()
{
}

void HttpRequest::reset()
{
    method.clear();
    target.clear();
    version.clear();
    requestPath.clear();
    query.clear();
    body.clear();
    headers.clear();
}


const std::string &HttpRequest::getBody() const { return body;}
const std::string &HttpRequest::getQuery() const { return query;}
const std::string &HttpRequest::getMethod() const { return method;}
const std::string &HttpRequest::getTarget() const { return target;}
const std::string &HttpRequest::getVersion() const { return version;}

void HttpRequest::setQuery(const std::string &value) { query = value;}
void HttpRequest::setTarget(const std::string &value) {target = value;}
void HttpRequest::setMethod(const std::string &value) {method = value;}
void HttpRequest::appendBody(const std::string &value) { body += value;}
void HttpRequest::setVersion(const std::string &value) { version = value;}

const std::string &HttpRequest::getRequestPath() const { return requestPath;}
void HttpRequest::setRequestPath(const std::string &value) { requestPath = value;}
const std::map<std::string, std::string> &HttpRequest::getHeaders() const { return headers;}
void HttpRequest::setHeader(const std::string &key, const std::string &value) { headers[toLower(key)] = value;}




void HttpRequest::appendHeader(const std::string &key, const std::string &value)
{
    std::string normalizedKey = toLower(key);
    std::map<std::string, std::string>::iterator it = headers.find(normalizedKey);

    if (it == headers.end())
        headers[normalizedKey] = value;
    else
        it->second += ", " + value;
}


bool HttpRequest::hasHeader(const std::string &key) const
{
    return headers.find(toLower(key)) != headers.end();
}

bool HttpRequest::shouldCloseConnection() const
{
    std::string connection = toLower(getHeader("connection"));

    if (version == "HTTP/1.0")
        return connection != "keep-alive";

    if (version == "HTTP/1.1")
        return connection == "close";

    return true;
}


const std::string &HttpRequest::getHeader(const std::string &key) const
{
    std::map<std::string, std::string>::const_iterator it =
        headers.find(toLower(key));

    if (it != headers.end())
        return it->second;

    static const std::string empty;
    return empty;
}


#include "HttpHandler.hpp"

#include "../Helpers.hpp"
#include "./Methods/Methods.hpp"
#include "HttpUtils/HttpUtils.hpp"

HttpHandler::HttpHandler(const ServerConfig &config): serverConfig(&config) {}

const Location *HttpHandler::matchLocation(const std::string &path) const
{
    const Location *bestMatch = NULL;
    size_t bestLength = 0;

    for (size_t i = 0; i < serverConfig->locations.size(); ++i)
    {
        const Location &location = serverConfig->locations[i];

        if (path.size() < location.path.size())
            continue;

        if (path.compare(0, location.path.size(), location.path) != 0)
            continue;

        bool validBoundary = location.path == "/" || path.size() == location.path.size() || location.path[location.path.size() - 1] == '/'
            || path[location.path.size()] == '/';

        if (!validBoundary)
            continue;

        if (location.path.size() > bestLength)
        {
            bestMatch = &location;
            bestLength = location.path.size();
        }
    }
    return bestMatch;
}

bool HttpHandler::isMethodAllowed(const std::string &method, const Location *location) const
{
    if (!location)
        return method == "GET";

    if (location->methods.empty())
        return true;

    for (size_t i = 0; i < location->methods.size(); ++i)
    {
        if (toUpper(location->methods[i]) == method)
            return true;
    }

    return false;
}

bool HttpHandler::isCgiRequest(const RouteMatch &match) const
{
    if (!match.location || match.location->cgiExt.empty())
        return false;

    const std::string &extension = match.location->cgiExt;

    if (match.fullPath.size() < extension.size())
        return false;

    size_t offset = match.fullPath.size() - extension.size();
    return match.fullPath.compare(offset, extension.size(), extension) == 0;
}

void HttpHandler::resolveRoute(const HttpRequest &request, RouteMatch &match) const
{
    match.requestPath = request.getRequestPath();
    match.location = matchLocation(match.requestPath);

    if (!match.location)
    {
        match.root = serverConfig->root;
        match.fullPath = joinPath(match.root, match.requestPath);
        return;
    }

    match.root = match.location->root;

    if (match.root.empty()) match.root = serverConfig->root;

    std::string relativePath = match.requestPath;
    if (match.location->path != "/")
    {
        relativePath = match.requestPath.substr(match.location->path.size());

        if (relativePath.empty())
            relativePath = "/";

        else if (relativePath[0] != '/')
            relativePath = "/" + relativePath;
    }
    match.fullPath = joinPath(match.root, relativePath);
}

std::vector<std::string> HttpHandler::resolveIndexFiles(const Location *location) const
{
    if (location && !location->indexes.empty())
        return location->indexes;

    if (!serverConfig->indexes.empty())
        return serverConfig->indexes;

    std::vector<std::string> defaults;
    defaults.push_back("index.html");
    return defaults;
}

HttpResponse HttpHandler::resolveRedirection(const Location &location) const
{
    std::string reason;
    switch (location.redirectCode)
    {
        case 301:
            reason = "Moved Permanently";
            break;
        case 302:
            reason = "Found";
            break;
        case 303:
            reason = "See Other";
            break;
        case 307:
            reason = "Temporary Redirect";
            break;
        case 308:
            reason = "Permanent Redirect";
            break;
        default:
            return ErrorPage(500, *serverConfig);
    }

    if (location.redirectTarget.empty())
        return ErrorPage(500, *serverConfig);

    HttpResponse response(location.redirectCode, reason);
    response.setHeader("Location", location.redirectTarget);
    response.setHeader("Content-Length", "0");
    return response;
}

bool HttpHandler::resolveDirectory(RouteMatch &match, const std::string &method, HttpResponse &response) const
{
    if (!isDirectory(match.fullPath))
        return true;
    std::vector<std::string> indexes = resolveIndexFiles(match.location);

    for (size_t i = 0; i < indexes.size(); ++i)
    {
        std::string candidate =
            joinPath(match.fullPath, indexes[i]);

        if (!fileExists(candidate))
            continue;

        match.requestPath = joinPath(match.requestPath, indexes[i]);
        match.fullPath = candidate;
        return true;
    }
    bool autoIndexEnabled = match.location && match.location->autoindex == "on";

    if (method == "GET" && autoIndexEnabled)
    {
        response = resolveAutoIndexing(match, *serverConfig);
        return false;
    }
    response = ErrorPage(403, *serverConfig);
    return false;
}

HttpResult HttpHandler::process(const HttpRequest &request) const
{
    RouteMatch match;
    resolveRoute(request, match);

    if (match.location && match.location->redirectCode != 0)
    {
        return HttpResult::makeResponse(resolveRedirection(*match.location));
    }
    const std::string &method = request.getMethod();

    if (!isMethodAllowed(method, match.location))
    {
        return HttpResult::makeResponse(ErrorPage(405, *serverConfig));
    }
    if (request.getBody().size() > static_cast<size_t>(serverConfig->client_max_body_size))
    {
        return HttpResult::makeResponse(ErrorPage(413, *serverConfig));
    }
    HttpResponse directoryResponse;

    if (!resolveDirectory(match, method, directoryResponse))
        return HttpResult::makeResponse(directoryResponse);

    if (isCgiRequest(match))
    {
        return HttpResult::makeCgi(match.location, match.fullPath);
    }

    if (!fileExists(match.fullPath) && method != "POST")
    {
        return HttpResult::makeResponse(ErrorPage(404, *serverConfig));
    }
    if (method == "GET")
    {
        return HttpResult::makeResponse(HttpMethods::GET(match, *serverConfig));
    }
    if (method == "DELETE")
    {
        return HttpResult::makeResponse(HttpMethods::DELETE(match, *serverConfig));
    }
    if (method == "POST")
    {
        return HttpResult::makeResponse(HttpMethods::POST(request, match, *serverConfig));
    }

    return HttpResult::makeResponse(
        ErrorPage(501, *serverConfig)
    );
}
#include "HttpRequestParser.hpp"

#include "../Helpers.hpp"
#include "HttpUtils/HttpUtils.hpp"

#include <cctype>
#include <sstream>
#include <algorithm>


HttpRequestParser::HttpRequestParser() : maxBodySize(0), parsedSize(0), state(PARSE_REQUEST_LINE), errorCode(0) {}

HttpRequestParser::~HttpRequestParser() {}


HttpRequest &HttpRequestParser::getRequest() { return request; }
int HttpRequestParser::getErrorCode() const { return errorCode; }
size_t HttpRequestParser::getParsedSize() const { return parsedSize; }
const HttpRequest &HttpRequestParser::getRequest() const { return request; }
void HttpRequestParser::setMaxBodySize(size_t value) { maxBodySize = value; }

bool urlDecode(const std::string &input, std::string &output)
{
    output.clear();
    output.reserve(input.size());

    for (size_t i = 0; i < input.size(); ++i)
    {
        if (input[i] != '%')
        {
            output += input[i];
            continue;
        }

        if (i + 2 >= input.size())
            return false;

        unsigned char first =
            static_cast<unsigned char>(input[i + 1]);

        unsigned char second =
            static_cast<unsigned char>(input[i + 2]);

        if (!std::isxdigit(first) || !std::isxdigit(second))
            return false;

        std::string hexValue = input.substr(i + 1, 2);
        std::istringstream stream(hexValue);

        int decodedValue = 0;
        stream >> std::hex >> decodedValue;

        if (stream.fail() || !stream.eof())
            return false;

        output += static_cast<char>(decodedValue);
        i += 2;
    }

    return true;
}

bool parseDecimalSize(const std::string &value, size_t &result)
{
    if (value.empty())
        return false;

    for (size_t i = 0; i < value.size(); ++i)
    {
        unsigned char character =
            static_cast<unsigned char>(value[i]);

        if (!std::isdigit(character))
            return false;
    }

    std::istringstream stream(value);
    size_t parsedValue = 0;

    stream >> parsedValue;

    if (stream.fail() || !stream.eof())
        return false;

    result = parsedValue;
    return true;
}

bool parseHexSize(const std::string &value, size_t &result)
{
    if (value.empty())
        return false;

    for (size_t i = 0; i < value.size(); ++i)
    {
        unsigned char character =
            static_cast<unsigned char>(value[i]);

        if (!std::isxdigit(character))
            return false;
    }

    std::istringstream stream(value);
    size_t parsedValue = 0;

    stream >> std::hex >> parsedValue;

    if (stream.fail() || !stream.eof())
        return false;

    result = parsedValue;
    return true;
}

void HttpRequestParser::setError(int code)
{
    errorCode = code;
    state = PARSE_ERROR;
}

void HttpRequestParser::reset()
{
    request.reset();

    maxBodySize = 0;
    parsedSize = 0;
    errorCode = 0;
    state = PARSE_REQUEST_LINE;
}

bool HttpRequestParser::splitTarget()
{
    const std::string &target = request.getTarget();

    size_t queryPosition = target.find('?');
    std::string rawPath;
    std::string query;

    if (queryPosition == std::string::npos)
    {
        rawPath = target;
        query.clear();
    }
    else
    {
        rawPath = target.substr(0, queryPosition);
        query = target.substr(queryPosition + 1);
    }

    std::string requestPath;

    if (!urlDecode(rawPath, requestPath))
    {
        setError(400);
        return false;
    }

    requestPath = normalizePath(requestPath);

    request.setRequestPath(requestPath);
    request.setQuery(query);

    return true;
}

StepStatus HttpRequestParser::parseRequestLine(const std::string &raw)
{
    size_t lineEnd = raw.find("\r\n", parsedSize);

    if (lineEnd == std::string::npos)
        return STEP_NEED_MORE_DATA;

    std::string line = raw.substr(parsedSize, lineEnd - parsedSize);
    std::istringstream lineStream(line);

    std::string method;
    std::string target;
    std::string version;
    std::string extraValue;

    if (!(lineStream >> method >> target >> version)
        || (lineStream >> extraValue))
    {
        setError(400);
        return STEP_ERROR;
    }

    if (target.empty() || target[0] != '/')
    {
        setError(400);
        return STEP_ERROR;
    }

    request.setMethod(toUpper(method));
    request.setTarget(target);
    request.setVersion(version);

    if (!splitTarget())
        return STEP_ERROR;

    parsedSize = lineEnd + 2;
    state = PARSE_HEADERS;

    return STEP_COMPLETE;
}

StepStatus HttpRequestParser::parseHeaders(const std::string &raw)
{
    size_t headersEnd = raw.find("\r\n\r\n", parsedSize);

    if (headersEnd == std::string::npos)
        return STEP_NEED_MORE_DATA;

    std::string headerSection =
        raw.substr(parsedSize, headersEnd - parsedSize);

    std::istringstream headerStream(headerSection);
    std::string line;

    while (std::getline(headerStream, line))
    {
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);

        if (line.empty())
            continue;

        size_t delimiterPosition = line.find(':');

        if (delimiterPosition == std::string::npos)
        {
            setError(400);
            return STEP_ERROR;
        }

        std::string key =
            toLower(trim(line.substr(0, delimiterPosition)));

        std::string value =
            trim(line.substr(delimiterPosition + 1));

        if (key.empty())
        {
            setError(400);
            return STEP_ERROR;
        }

        if ((key == "content-length" || key == "host")
            && request.hasHeader(key))
        {
            setError(400);
            return STEP_ERROR;
        }

        if (request.hasHeader(key))
            request.appendHeader(key, value);
        else
            request.setHeader(key, value);
    }

    const std::string &version = request.getVersion();

    if (version != "HTTP/1.1" && version != "HTTP/1.0")
    {
        setError(505);
        return STEP_ERROR;
    }

    if (version == "HTTP/1.1" && !request.hasHeader("host"))
    {
        setError(400);
        return STEP_ERROR;
    }

    parsedSize = headersEnd + 4;
    state = PARSE_BODY;

    return STEP_COMPLETE;
}

StepStatus HttpRequestParser::parseChunkedBody(const std::string &raw)
{
    const std::string crlf = "\r\n";
    const size_t crlfSize = crlf.size();

    while (true)
    {
        size_t chunkHeaderEnd = raw.find(crlf, parsedSize);

        if (chunkHeaderEnd == std::string::npos)
            return STEP_NEED_MORE_DATA;

        std::string chunkHeader =
            raw.substr(parsedSize, chunkHeaderEnd - parsedSize);

        size_t extensionPosition = chunkHeader.find(';');

        if (extensionPosition != std::string::npos)
            chunkHeader = chunkHeader.substr(0, extensionPosition);

        chunkHeader = trim(chunkHeader);

        size_t chunkSize = 0;

        if (!parseHexSize(chunkHeader, chunkSize))
        {
            setError(400);
            return STEP_ERROR;
        }

        size_t dataStart = chunkHeaderEnd + crlfSize;

        if (chunkSize == 0)
        {
            if (raw.size() < dataStart + crlfSize)
                return STEP_NEED_MORE_DATA;

            if (raw.compare(dataStart, crlfSize, crlf) != 0)
            {
                setError(400);
                return STEP_ERROR;
            }

            parsedSize = dataStart + crlfSize;
            return STEP_COMPLETE;
        }

        if (maxBodySize > 0)
        {
            size_t currentBodySize = request.getBody().size();

            if (currentBodySize > maxBodySize
                || chunkSize > maxBodySize - currentBodySize)
            {
                setError(413);
                return STEP_ERROR;
            }
        }

        if (dataStart > raw.size())
            return STEP_NEED_MORE_DATA;

        if (chunkSize > raw.size() - dataStart)
            return STEP_NEED_MORE_DATA;

        size_t chunkEndingPosition = dataStart + chunkSize;

        if (raw.size() < chunkEndingPosition + crlfSize)
            return STEP_NEED_MORE_DATA;

        if (raw.compare(chunkEndingPosition, crlfSize, crlf) != 0)
        {
            setError(400);
            return STEP_ERROR;
        }

        request.appendBody(raw.substr(dataStart, chunkSize));
        parsedSize = chunkEndingPosition + crlfSize;
    }
}

StepStatus HttpRequestParser::parseBody(const std::string &raw)
{
    std::string transferEncoding =
        toLower(trim(request.getHeader("transfer-encoding")));

    std::string contentLengthHeader =
        trim(request.getHeader("content-length"));

    if (!transferEncoding.empty() && !contentLengthHeader.empty())
    {
        setError(400);
        return STEP_ERROR;
    }

    if (!transferEncoding.empty())
    {
        if (transferEncoding != "chunked")
        {
            setError(501);
            return STEP_ERROR;
        }

        StepStatus chunkStatus = parseChunkedBody(raw);

        if (chunkStatus != STEP_COMPLETE)
            return chunkStatus;
    }
    else if (!contentLengthHeader.empty())
    {
        size_t contentLength = 0;

        if (!parseDecimalSize(contentLengthHeader, contentLength))
        {
            setError(400);
            return STEP_ERROR;
        }

        if (maxBodySize > 0 && contentLength > maxBodySize)
        {
            setError(413);
            return STEP_ERROR;
        }

        size_t currentBodySize = request.getBody().size();

        if (currentBodySize > contentLength)
        {
            setError(400);
            return STEP_ERROR;
        }

        if (parsedSize > raw.size())
        {
            setError(400);
            return STEP_ERROR;
        }

        size_t neededBytes = contentLength - currentBodySize;
        size_t availableBytes = raw.size() - parsedSize;
        size_t copiedBytes = std::min(neededBytes, availableBytes);

        if (copiedBytes > 0)
        {
            request.appendBody(raw.substr(parsedSize, copiedBytes));
            parsedSize += copiedBytes;
        }

        if (request.getBody().size() < contentLength)
            return STEP_NEED_MORE_DATA;
    }

    state = PARSE_COMPLETE;
    return STEP_COMPLETE;
}

ParseStatus HttpRequestParser::parse(const std::string &rawRequestData)
{
    while (state != PARSE_COMPLETE && state != PARSE_ERROR)
    {
        State previousState = state;
        StepStatus stepStatus = STEP_ERROR;

        switch (state)
        {
            case PARSE_REQUEST_LINE:
                stepStatus = parseRequestLine(rawRequestData);
                break;

            case PARSE_HEADERS:
                stepStatus = parseHeaders(rawRequestData);
                break;

            case PARSE_BODY:
                stepStatus = parseBody(rawRequestData);
                break;

            default:
                setError(500);
                return PARSE_REQUEST_ERROR;
        }

        if (stepStatus == STEP_NEED_MORE_DATA)
            return PARSE_NEED_MORE_DATA;

        if (stepStatus == STEP_ERROR)
            return PARSE_REQUEST_ERROR;

        if (previousState == PARSE_HEADERS && state == PARSE_BODY)
            return PARSE_HEADERS_COMPLETE;
    }

    if (state == PARSE_ERROR)
        return PARSE_REQUEST_ERROR;

    return PARSE_REQUEST_COMPLETE;
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

#include "./Server.hpp"
#include "../CGI/CGI.hpp"
#include "../HTTP/HttpUtils/HttpUtils.hpp"
#include "../HTTP/HttpHandler.hpp"
#include "../HTTP/HttpRequestParser.hpp"

#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <ctime>

Client::Client(int fd, Server *srv, const std::vector<ServerConfig> &confs)
    : socketFD(fd),
      server(srv),
      configs(confs),
      activeConfig(NULL),
      requestParser(),
      activeCgi(NULL),
      state(READING_REQUEST),
      closeAfterWrite(false),
      timeout(time(NULL)) {}

Client::~Client()
{
    if (socketFD >= 0)
    {
        ::close(socketFD);
        socketFD = -1;
    }

    activeCgi = NULL;
}

void Client::consumeReadBuffer(size_t bytes)
{
    if (bytes >= readBuffer.size())
        readBuffer.clear();
    else
        readBuffer.erase(0, bytes);
}

ClientState Client::getState() const
{
    return state;
}

void Client::consumeWriteBuffer(size_t bytes)
{
    if (bytes >= writeBuffer.size())
        writeBuffer.clear();
    else
        writeBuffer.erase(0, bytes);
}

int Client::getFD() const
{
    return socketFD;
}

void Client::appendToWriteBuffer(const std::string &data)
{
    writeBuffer += data;
}

void Client::appendToReadBuffer(const char *data, size_t size)
{
    readBuffer.append(data, size);
}

HttpRequest &Client::getActiveRequest()
{
    return requestParser.getRequest();
}

bool Client::isConnected() const
{
    return socketFD >= 0;
}

bool Client::hasPendingWrite() const
{
    return !writeBuffer.empty();
}

const std::string &Client::getReadBuffer() const
{
    return readBuffer;
}

const std::string &Client::getWriteBuffer() const
{
    return writeBuffer;
}

const ServerConfig *Client::matchConfig(const std::string &rawHost) const
{
    if (configs.empty())
        return NULL;

    std::string host = rawHost;

    if (!host.empty() && host[host.size() - 1] == '\r')
        host.erase(host.size() - 1);

    size_t portSeparator = host.find(':');

    if (portSeparator != std::string::npos)
        host = host.substr(0, portSeparator);

    host = toLower(host);

    for (size_t i = 0; i < configs.size(); ++i)
    {
        for (size_t j = 0; j < configs[i].serverNames.size(); ++j)
        {
            if (toLower(configs[i].serverNames[j]) == host)
                return &configs[i];
        }
    }

    return &configs[0];
}

void Client::closeConnection()
{
    if (activeCgi)
    {
        CGI *cgi = activeCgi;
        activeCgi = NULL;

        cgi->killCgi();
        server->removeHandler(cgi->getFD());
    }

    server->removeHandler(socketFD);
}

void Client::terminateCgi()
{
    if (!activeCgi)
        return;

    activeCgi->killCgi();

    const ServerConfig *config = activeConfig;

    if (!config && !configs.empty())
        config = &configs[0];

    if (!config)
    {
        closeConnection();
        return;
    }

    HttpResponse response = ErrorPage(504, *config);

    response.setHeader("Connection", "close");
    closeAfterWrite = true;

    int cgiFD = activeCgi->getFD();

    activeCgi = NULL;
    server->removeHandler(cgiFD);

    writeBuffer = response.toString();

    requestParser.reset();
    readBuffer.clear();

    state = SENDING_RESPONSE;
    server->modifyHandler(this, EPOLLOUT);
}

void Client::onCgiDone(HttpResponse response)
{
    HttpRequest &request = requestParser.getRequest();

    closeAfterWrite = request.shouldCloseConnection();

    if (closeAfterWrite)
        response.setHeader("Connection", "close");

    if (activeCgi)
    {
        int cgiFD = activeCgi->getFD();

        activeCgi = NULL;
        server->removeHandler(cgiFD);
    }

    writeBuffer = response.toString();

    requestParser.reset();

    state = SENDING_RESPONSE;
    server->modifyHandler(this, EPOLLOUT);
}

void Client::errorsHandler(int errorCode)
{
    const ServerConfig *config = activeConfig;

    if (!config && !configs.empty())
        config = &configs[0];

    if (!config)
    {
        closeConnection();
        return;
    }

    HttpResponse response = ErrorPage(errorCode, *config);

    response.setHeader("Connection", "close");
    closeAfterWrite = true;

    writeBuffer = response.toString();

    requestParser.reset();
    readBuffer.clear();

    state = SENDING_RESPONSE;
    server->modifyHandler(this, EPOLLOUT);
}

void Client::handleRead()
{
    if (state != READING_REQUEST)
        return;

    char buffer[8192];
    bool receivedData = false;

    while (true)
    {
        ssize_t bytes = recv(socketFD, buffer, sizeof(buffer), 0);

        if (bytes > 0)
        {
            appendToReadBuffer(buffer, static_cast<size_t>(bytes));
            receivedData = true;
            timeout = time(NULL);
            continue;
        }

        if (bytes == 0)
        {
            closeConnection();
            return;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK)
            break;

        closeConnection();
        return;
    }

    if (!receivedData)
        return;

    while (state == READING_REQUEST)
    {
        ParseStatus parseStatus = requestParser.parse(readBuffer);
        HttpRequest &request = requestParser.getRequest();

        if (parseStatus == PARSE_REQUEST_ERROR)
        {
            errorsHandler(requestParser.getErrorCode());
            return;
        }

        switch (parseStatus)
        {
            case PARSE_HEADERS_COMPLETE:
            {
                activeConfig = matchConfig(request.getHeader("host"));

                if (!activeConfig)
                {
                    closeConnection();
                    return;
                }

                requestParser.setMaxBodySize(activeConfig->client_max_body_size);

                std::string contentLength =
                    request.getHeader("content-length");

                if (!contentLength.empty()
                    && myStold(contentLength) > activeConfig->client_max_body_size)
                {
                    errorsHandler(413);
                    return;
                }

                continue;
            }

            case PARSE_NEED_MORE_DATA:
                return;

            case PARSE_REQUEST_COMPLETE:
            {
                if (!activeConfig)
                {
                    if (configs.empty())
                    {
                        closeConnection();
                        return;
                    }

                    activeConfig = &configs[0];
                }

                HttpHandler handler(*activeConfig);
                HttpResult result = handler.process(request);

                size_t consumedBytes = requestParser.getParsedSize();
                closeAfterWrite = request.shouldCloseConnection();

                consumeReadBuffer(consumedBytes);

                if (result.type == HTTP_RESULT_CGI)
                {
                    state = PROCESSING_CGI;

                    activeCgi = new CGI(this, server, request,
                        *result.cgiLocation, result.cgiRequestPath);

                    server->addHandler(activeCgi, EPOLLOUT);
                    return;
                }

                HttpResponse response = result.response;

                if (closeAfterWrite)
                    response.setHeader("Connection", "close");

                writeBuffer = response.toString();

                requestParser.reset();

                state = SENDING_RESPONSE;
                server->modifyHandler(this, EPOLLOUT);
                return;
            }

            case PARSE_REQUEST_ERROR:
                errorsHandler(requestParser.getErrorCode());
                return;
        }
    }
}

void Client::handleWrite()
{
    while (hasPendingWrite())
    {
        ssize_t bytes = send(socketFD, writeBuffer.c_str(), writeBuffer.size(), 0);
        if (bytes > 0)
        {
            consumeWriteBuffer(static_cast<size_t>(bytes));
            timeout = time(NULL);
            continue;
        }
        if (bytes == 0)
            return;
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return;
        closeConnection();
        return;
    }
    if (closeAfterWrite)
    {
        closeConnection();
        return;
    }

    closeAfterWrite = false;
    activeConfig = NULL;
    state = READING_REQUEST;
    server->modifyHandler(this, EPOLLIN);
}

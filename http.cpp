#include "RequestParser.hpp"

#include "../HttpUtils/HttpUtils.hpp"

RequestParser::RequestParser(const std::vector<ServerConfig *> &conf)
    :state(PARSE_REQUEST_LINE),
    errorCode(0),
    parsedSize(0),
    maxBodySize(0),
    expectedBodySize(0),
    bodySizeInitialized(false),
    activeConfig(NULL),
    configs(conf) {}

RequestParser::~RequestParser() {}

HttpRequest &RequestParser::getRequest()
{
    return request;
}

const HttpRequest &RequestParser::getRequest() const
{
    return request;
}

int RequestParser::getErrorCode() const
{
    return errorCode;
}

size_t RequestParser::getParsedSize() const
{
    return parsedSize;
}

void RequestParser::resetParsedSize()
{
    parsedSize = 0;
}

const ServerConfig *RequestParser::getActiveConfig()
{
    return activeConfig;
}

void RequestParser::setMaxBodySize(size_t value)
{
    maxBodySize = value;
}

void RequestParser::setError(int code)
{
    errorCode = code;
    state = PARSE_ERROR;
}

void RequestParser::reset()
{
    request.reset();

    maxBodySize = 0;
    parsedSize = 0;
    expectedBodySize = 0;
    bodySizeInitialized = false;
    state = PARSE_REQUEST_LINE;
    errorCode = 0;
    activeConfig = NULL;
}

bool RequestParser::initializeRequestConfig()
{
    if (configs.empty())
    {
        setError(500);
        return false;
    }

    std::string host;

    if (request.hasHost())
        host = request.getHost();

    activeConfig = matchConfig(configs, host);

    if (!activeConfig)
    {
        setError(500);
        return false;
    }

    const Location *location = matchLocation(*activeConfig, request.getRequestPath());

    maxBodySize = activeConfig->client_max_body_size;

    if (location)
        maxBodySize = location->client_max_body_size;

    if (request.hasContentLength()
        && maxBodySize > 0
        && request.getContentLength() > maxBodySize)
    {
        setError(413);
        return false;
    }

    return true;
}

ParseStatus RequestParser::parse(const std::string &rawRequestData)
{
    while (state != PARSE_COMPLETE && state != PARSE_ERROR)
    {
        State previousState = state;
        StepStatus stepStatus = STEP_ERROR;

        switch (state)
        {
            case PARSE_REQUEST_LINE:
                stepStatus = requestLineParser(rawRequestData);
                break;

            case PARSE_HEADERS:
                stepStatus = headersParser(rawRequestData);
                break;

            case PARSE_BODY:
                stepStatus = bodyParser(rawRequestData);
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
        {
            if (!initializeRequestConfig())
                return PARSE_REQUEST_ERROR;
        }
    }

    if (state == PARSE_ERROR)
        return PARSE_REQUEST_ERROR;

    if (state == PARSE_COMPLETE)
        return PARSE_REQUEST_COMPLETE;

    return PARSE_NEED_MORE_DATA;
}


#include "HttpHandler.hpp"

#include "../Helpers.hpp"
#include "./Methods/Methods.hpp"
#include "HttpUtils/HttpUtils.hpp"

HttpHandler::HttpHandler(const ServerConfig &config): serverConfig(&config) {}


bool HttpHandler::isMethodAllowed(const Location *location, const std::string &method) const
{
    if (!location)
        return method == "GET";

    return std::find(location->methods.begin(), location->methods.end(), method) != location->methods.end();
}

bool HttpHandler::isCgiRequest(const RouteMatch &match) const
{
    if (!match.location || match.location->cgiMappings.empty())
        return false;
    
    size_t dotPos = match.fullPath.find_last_of('.');
    if (dotPos == std::string::npos)
        return false;
    
    std::string extension = match.fullPath.substr(dotPos);
    return match.location->cgiMappings.find(extension) != match.location->cgiMappings.end();
}

void HttpHandler::resolveRoute(const HttpRequest &request, RouteMatch &match) const
{
    match.requestPath = request.getRequestPath();
    match.location = matchLocation(*serverConfig, match.requestPath);

    if (match.location && !match.location->root.empty())
    {
        match.root = match.location->root;

        std::string relativePath = match.requestPath.substr(match.location->path.size());
        if (relativePath.empty())
            relativePath = "/";

        match.fullPath = joinPath(match.root, relativePath);
        return;
    }
    match.root = serverConfig->root;
    match.fullPath = joinPath(match.root, match.requestPath);
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

HttpResult HttpHandler::resolveCgi(const RouteMatch &match) const
{
    size_t dotPos = match.fullPath.find_last_of('.');

    if (dotPos == std::string::npos)
        return HttpResult::makeResponse(ErrorPage(500, *serverConfig));

    std::string extension = match.fullPath.substr(dotPos);

    std::map<std::string, std::string>::const_iterator it;
    it = match.location->cgiMappings.find(extension);

    if (it == match.location->cgiMappings.end())
        return HttpResult::makeResponse(ErrorPage(500, *serverConfig));

    return HttpResult::makeCgi(match.location, match.fullPath, it->second);
}

bool HttpHandler::resolveDirectory(RouteMatch &match, const HttpRequest &request, HttpResponse &response) const
{
    if (request.getMethod() == "POST")
        return true;
    if (!isDirectory(match.fullPath))
        return true;

    if (request.getMethod() == "DELETE")
    {
        response = ErrorPage(403, *serverConfig);
        return false;
    } 
    if (match.requestPath.empty() || match.requestPath[match.requestPath.size() - 1] != '/')
    {
        std::string queryString = request.getQuery();
        std::string redirectTarget = match.requestPath + "/";

        if (!queryString.empty())
            redirectTarget += "?" + queryString;

        response = HttpResponse(301, "Moved Permanently");
        response.setHeader("Location", redirectTarget);
        response.setHeader("Content-Length", "0");
        return false;
    }

    std::vector<std::string> indexes = resolveIndexFiles(match.location);
    for (size_t i = 0; i < indexes.size(); ++i)
    {
        std::string candidate = joinPath(match.fullPath, indexes[i]);
        if (!isRegularFile(candidate))
            continue;
        match.requestPath = joinPath(match.requestPath, indexes[i]);
        match.fullPath = candidate;
        return true;
    }
    bool autoIndexEnabled = match.location && match.location->autoindex == "on";
    if (request.getMethod() == "GET" && autoIndexEnabled)
    {
        response = resolveAutoIndexing(match, *serverConfig);
        return false;
    }
    response = ErrorPage(404, *serverConfig);
    return false;
}

HttpResult HttpHandler::process(const HttpRequest &request) const
{
    std::cout << "[REQUEST]: " << request.getVersion() << " " << request.getMethod() << " " << request.getRequestPath() << std::endl;
    RouteMatch match;
    resolveRoute(request, match);
    std::cout << "[LOCATION]: " << (match.location ? match.location->path : "NONE") << std::endl;
    if (match.location && match.location->redirectCode != 0)
    {
        return HttpResult::makeResponse(resolveRedirection(*match.location));
    }
    const std::string &method = request.getMethod();
    if (!isMethodAllowed(match.location, method))
    {
        HttpResponse response = ErrorPage(405, *serverConfig);
        std::string allowedMethods;
        if (match.location)
        {
            for (size_t i = 0; i < match.location->methods.size(); ++i)
            {
                if (i != 0)
                    allowedMethods += ", ";
                allowedMethods += match.location->methods[i];
            }
        }
        else
            allowedMethods = "GET";
        response.setHeader("Allow", allowedMethods);
        return HttpResult::makeResponse(response);
    }
    HttpResponse directoryResponse;
    if (!resolveDirectory(match, request, directoryResponse))
        return HttpResult::makeResponse(directoryResponse);
    std::cout << "[MAP TO]: " << match.fullPath << std::endl;
    if (isCgiRequest(match))
    {
        return resolveCgi(match);
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
        std::cout << "hhhe from post\n";
        return HttpResult::makeResponse(HttpMethods::POST(request, match, *serverConfig));
    }
    return HttpResult::makeResponse(ErrorPage(501, *serverConfig));
}


#include "HttpResponse.hpp"

HttpResponse::HttpResponse() : statusCode(200), reasonPhrase("OK")
{
    setHeader("Connection", "keep-alive");
}

HttpResponse::HttpResponse(int code, const std::string &reason)
    : statusCode(code), reasonPhrase(reason)
{
    setHeader("Connection", "keep-alive");
}

HttpResponse::~HttpResponse()
{
}

void HttpResponse::setHeader(const std::string &key, const std::string &value)
{
    headers[key].clear();
    headers[key].push_back(value);
}

void HttpResponse::addHeader(const std::string &key, const std::string &value)
{
    headers[key].push_back(value);
}

void HttpResponse::setBody(const std::string &content)
{
    body = content;

    std::ostringstream sizeStream;
    sizeStream << body.size();

    setHeader("Content-Length", sizeStream.str());
}

void HttpResponse::writeBody(const std::string &chunk)
{
    body += chunk;

    std::ostringstream sizeStream;
    sizeStream << body.size();

    setHeader("Content-Length", sizeStream.str());
}

std::string HttpResponse::toString() const
{
    std::ostringstream responseStream;

    responseStream << "HTTP/1.1 " << statusCode << " "
                   << reasonPhrase << "\r\n";

    std::cout << "[RESPONSE]: HTTP/1.1 " << statusCode << " "
        << reasonPhrase << std::endl;
    std::map<std::string, std::vector<std::string> >::const_iterator headerIterator;

    for (headerIterator = headers.begin(); headerIterator != headers.end(); ++headerIterator)
    {
        const std::vector<std::string> &values = headerIterator->second;

        for (size_t i = 0; i < values.size(); ++i)
        {
            responseStream << headerIterator->first << ": "
                           << values[i] << "\r\n";
        }
    }

    responseStream << "\r\n";
    responseStream << body;

    return responseStream.str();
}

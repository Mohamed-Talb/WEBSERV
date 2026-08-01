#include "RequestParser.hpp"

int splitTarget(HttpRequest &request)
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
        return 400;

    bool hadTrailingSlash = requestPath.size() > 1 && requestPath[requestPath.size() - 1] == '/';

    std::string normalizedPath;
    if (!normalizePath(requestPath, normalizedPath))
    {
        return 403;
    }
    request.setRequestPath(normalizedPath);

    if (hadTrailingSlash && requestPath.size() > 1 && requestPath[requestPath.size() - 1] != '/')
    {
        requestPath += "/";
    }
    if (requestPath.empty())
        requestPath = "/";

    request.setRequestPath(requestPath);
    request.setQuery(query);

    return true;
}

bool parseRequestLineValues(const std::string &line, std::string &method, std::string &target, std::string &version)
{
    std::istringstream lineStream(line);
    std::string extraValue;

    if (!(lineStream >> method >> target >> version))
        return false;

    if (lineStream >> extraValue)
        return false;

    return true;
}

bool validateRequestTarget(const std::string &target)
{
    return !target.empty() && target[0] == '/';
}

StepStatus RequestParser::requestLineParser(const std::string &raw)
{
    size_t lineEnd = raw.find("\r\n", parsedSize);

    if (lineEnd == std::string::npos)
    {
        if (raw.size() - parsedSize > MAX_REQUEST_LINE_SIZE)
        {
            setError(414);
            return STEP_ERROR;
        }

        return STEP_NEED_MORE_DATA;
    }

    if (lineEnd - parsedSize > MAX_REQUEST_LINE_SIZE)
    {
        setError(414);
        return STEP_ERROR;
    }

    std::string line = raw.substr(parsedSize, lineEnd - parsedSize);
    std::string method;
    std::string target;
    std::string version;

    if (!parseRequestLineValues(line, method, target, version))
    {
        setError(400);
        return STEP_ERROR;
    }

    if (!validateRequestTarget(target))
    {
        setError(400);
        return STEP_ERROR;
    }

    request.setMethod(method);
    request.setTarget(target);
    request.setVersion(version);
    int ret = splitTarget(request);
    if (ret > 0)
    {
        setError(ret);
        return STEP_ERROR;
    }

    parsedSize = lineEnd + 2;
    state = PARSE_HEADERS;

    return STEP_COMPLETE;
}
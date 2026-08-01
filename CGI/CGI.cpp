#include "CGI.hpp"
#include "../Server/Client.hpp"
#include "../Server/Server.hpp"


CGI::CGI(
    Client *client,
    Server *srv,
    const HttpRequest &request,
    const std::string &fullResolvedPath,
    const std::string &interpreter,
    const ServerConfig *config
)
    : pipeInFd(-1),
      pipeOutFd(-1),
      cgiPid(-1),
      writeOffset(0),
      state(WRITING_INPUT),
      server(srv),
      parentClient(client),
      execBin(interpreter),
      config(config)
{
    if (!server || !parentClient)
        throw std::runtime_error("CGI: invalid server or client");

    parentClient->lastAction = std::time(NULL);
    requestBody = request.getBody();

    char **envp = buildEnv(request, fullResolvedPath);

    int pipeIn[2] = {-1, -1};
    int pipeOut[2] = {-1, -1};

    if (pipe(pipeIn) < 0)
    {
        freeEnv(envp);
        throw std::runtime_error("CGI: input pipe failed");
    }

    if (pipe(pipeOut) < 0)
    {
        close(pipeIn[0]);
        close(pipeIn[1]);
        freeEnv(envp);
        throw std::runtime_error("CGI: output pipe failed");
    }

    int flags = fcntl(pipeIn[1], F_GETFL, 0);

    if (flags < 0 || fcntl(pipeIn[1], F_SETFL, flags | O_NONBLOCK) < 0)
    {
        close(pipeIn[0]);
        close(pipeIn[1]);
        close(pipeOut[0]);
        close(pipeOut[1]);
        freeEnv(envp);
        throw std::runtime_error("CGI: failed to configure input pipe");
    }

    flags = fcntl(pipeOut[0], F_GETFL, 0);

    if (flags < 0 || fcntl(pipeOut[0], F_SETFL, flags | O_NONBLOCK) < 0)
    {
        close(pipeIn[0]);
        close(pipeIn[1]);
        close(pipeOut[0]);
        close(pipeOut[1]);
        freeEnv(envp);
        throw std::runtime_error("CGI: failed to configure output pipe");
    }

    cgiPid = fork();

    if (cgiPid < 0)
    {
        close(pipeIn[0]);
        close(pipeIn[1]);
        close(pipeOut[0]);
        close(pipeOut[1]);
        freeEnv(envp);
        cgiPid = -1;
        throw std::runtime_error("CGI: fork failed");
    }

    if (cgiPid == 0)
    {
        if (dup2(pipeIn[0], STDIN_FILENO) < 0)
            exit(1);

        if (dup2(pipeOut[1], STDOUT_FILENO) < 0)
            exit(1);

        close(pipeIn[0]);
        close(pipeIn[1]);
        close(pipeOut[0]);
        close(pipeOut[1]);
        char *args[] = {
            const_cast<char *>(execBin.c_str()),
            const_cast<char *>(fullResolvedPath.c_str()),
            NULL
        };
        execve(args[0], args, envp);
        perror("CGI: execve");
        exit(1);
    }

    close(pipeIn[0]);
    close(pipeOut[1]);

    pipeInFd = pipeIn[1];
    pipeOutFd = pipeOut[0];

    freeEnv(envp);
}


char **CGI::buildEnv(const HttpRequest &request, const std::string &scriptPath)
{
    const std::map<std::string, std::vector<std::string> > &headers = request.getRawHeaders();
    std::vector<std::string> environment;

    std::string absoluteScriptPath = getAbsolutePath(scriptPath);
    if (absoluteScriptPath.empty())
        throw std::runtime_error("CGI: cannot resolve script path: " + scriptPath);

    std::string contentType;
    if (request.hasContentType())
        contentType = request.getContentType().raw;

    std::ostringstream lengthStream;
    lengthStream << request.getBody().size();

    std::string uri = request.getRequestPath();
    if (!request.getQuery().empty())
        uri += "?" + request.getQuery();

    std::string serverName;
    if (request.hasHost())
    {
        serverName = request.getHost();

        if (!serverName.empty() && serverName[0] == '[')
        {
            size_t closingBracket = serverName.find(']');
            if (closingBracket != std::string::npos)
                serverName = serverName.substr(1, closingBracket - 1);
        }
        else
        {
            size_t colon = serverName.find(':');
            if (colon != std::string::npos)
                serverName = serverName.substr(0, colon);
        }
    }

    environment.push_back("REQUEST_METHOD=" + request.getMethod());
    environment.push_back("REQUEST_URI=" + uri);
    environment.push_back("CONTENT_LENGTH=" + lengthStream.str());
    environment.push_back("CONTENT_TYPE=" + contentType);
    environment.push_back("SCRIPT_NAME=" + request.getRequestPath());
    environment.push_back("SCRIPT_FILENAME=" + absoluteScriptPath);
    environment.push_back("PATH_INFO=");
    environment.push_back("QUERY_STRING=" + request.getQuery());
    environment.push_back("GATEWAY_INTERFACE=CGI/1.1");
    environment.push_back("SERVER_PROTOCOL=" + request.getVersion());
    environment.push_back("SERVER_NAME=" + serverName);
    environment.push_back("SERVER_SOFTWARE=webserv");
    environment.push_back("REDIRECT_STATUS=200");

    for (std::map<std::string, std::vector<std::string> >::const_iterator it = headers.begin(); it != headers.end(); ++it)
    {
        const std::string &key = it->first;

        if (key == "content-length" || key == "content-type")
            continue;

        const std::vector<std::string> &values = it->second;
        std::string combined;
        std::string separator = ", ";

        if (key == "cookie")
            separator = "; ";

        for (size_t i = 0; i < values.size(); ++i)
        {
            std::string value = trim(values[i]);

            if (value.empty())
                continue;

            if (!combined.empty())
                combined += separator;

            combined += value;
        }

        if (combined.empty())
            continue;

        std::string envName = "HTTP_";

        for (size_t i = 0; i < key.size(); ++i)
        {
            unsigned char current = static_cast<unsigned char>(key[i]);

            if (current == '-')
                envName += '_';
            else
                envName += static_cast<char>(std::toupper(current));
        }

        environment.push_back(envName + "=" + combined);
    }

    char **envp = new char *[environment.size() + 1];

    for (size_t i = 0; i <= environment.size(); ++i)
        envp[i] = NULL;

    for (size_t i = 0; i < environment.size(); ++i)
    {
        envp[i] = strdup(environment[i].c_str());

        if (!envp[i])
        {
            for (size_t j = 0; j < i; ++j)
                free(envp[j]);

            delete[] envp;
            throw std::bad_alloc();
        }
    }

    return envp;
}

void CGI::freeEnv(char **envp)
{
    if (!envp)
        return;

    for (int i = 0; envp[i]; ++i)
        free(envp[i]);

    delete[] envp;
}

int CGI::getFD() const
{
    return -1;
}

void CGI::registerHandlers()
{
    server->addHandler(this, pipeOutFd, EPOLLIN);
    try
    {
        if (requestBody.empty())
        {
            close(pipeInFd);
            pipeInFd = -1;
            return;
        }
        server->addHandler(this, pipeInFd, EPOLLOUT);
    }
    catch (...)
    {
        server->unregisterFD(pipeOutFd);
        throw;
    }
}

CGI::~CGI()
{
    if (pipeInFd >= 0)
    {
        close(pipeInFd);
        pipeInFd = -1;
    }
    if (pipeOutFd >= 0)
    {
        close(pipeOutFd);
        pipeOutFd = -1;
    }
    if (cgiPid > 0)
    {
        pid_t pid = cgiPid;
        kill(pid, SIGKILL);
        while (waitpid(pid, NULL, 0) < 0)
        {
            if (errno == EINTR)
                continue;
            break;
        }
        cgiPid = -1;
    }
    parentClient = NULL;
}

void CGI::closeInput()
{
    if (pipeInFd < 0)
        return;

    server->unregisterFD(pipeInFd);
    close(pipeInFd);
    pipeInFd = -1;
}

void CGI::closeOutput()
{
    if (pipeOutFd < 0)
        return;

    server->unregisterFD(pipeOutFd);
    close(pipeOutFd);
    pipeOutFd = -1;
}

void CGI::killCgi()
{
    if (state == DONE)
        return;

    state = DONE;
    parentClient = NULL;

    closeInput();
    closeOutput();

    if (cgiPid > 0)
    {
        pid_t pid = cgiPid;
        kill(pid, SIGKILL);
        while (waitpid(pid, NULL, 0) < 0)
        {
            if (errno == EINTR)
                continue;
            break;
        }

        cgiPid = -1;
    }
    server->removeHandler(this);
}

void CGI::finish()
{
    if (state == DONE)
        return;

    state = DONE;

    Client *client = parentClient;
    parentClient = NULL;
    HttpResponse response = parseCgiOutput(rawOutputBuffer);
    if (cgiPid > 0)
    {
        waitpid(cgiPid, NULL, 0);
        cgiPid = -1;
    }
    if (client)
        client->onCgiDone(response);
    server->removeHandler(this);
}

HttpResponse CGI::parseCgiOutput(const std::string &rawOutput)
{
    size_t delimiter = rawOutput.find("\r\n\r\n");
    size_t delimiterLen = 4;
    if (delimiter == std::string::npos)
    {
        delimiter = rawOutput.find("\n\n");
        delimiterLen = 2;
    }
    if (delimiter == std::string::npos)
        return ErrorPage(500, *config);

    std::string headersPart = rawOutput.substr(0, delimiter);
    std::string bodyPart = rawOutput.substr(delimiter + delimiterLen);

    if (headersPart.find("HTTP/") == 0)
        return ErrorPage(500, *config);

    int statusCode = 200;
    std::string reasonPhrase = "OK";
    std::string contentType;
    bool contentTypeSet = false;
    std::vector<std::pair<std::string, std::string> > extraHeaders;

    std::istringstream headerStream(headersPart);
    std::string line;
    while (std::getline(headerStream, line))
    {
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);
        if (line.empty())
            continue;

        size_t colon = line.find(':');
        if (colon == std::string::npos)
            return ErrorPage(500, *config);

        std::string name = trim(line.substr(0, colon));
        std::string value = trim(line.substr(colon + 1));
        if (name.empty() || value.empty())
            return ErrorPage(500, *config);

        std::string lowerName = toLower(name);

        if (lowerName == "status")
        {
            std::istringstream statusStream(value);
            int code = 0;
            std::string reason;
            if (!(statusStream >> code) || code < 200 || code >= 600)
                return ErrorPage(500, *config);
            std::getline(statusStream >> std::ws, reason);
            statusCode = code;
            if (reason.empty())
                reasonPhrase = getReasonPhrase(code);
            else
                reasonPhrase = reason;
        }
        else if (lowerName == "content-type")
        {
            if (contentTypeSet)
                return ErrorPage(500, *config);
            contentType = value;
            contentTypeSet = true;
        }
        else if (lowerName == "content-length" ||
                 lowerName == "connection" ||
                 lowerName == "transfer-encoding" ||
                 lowerName == "keep-alive")
        {
            continue;
        }
        else
        {
            extraHeaders.push_back(std::make_pair(name, value));
        }
    }

    if (!contentTypeSet)
        return ErrorPage(500, *config);

    HttpResponse response(statusCode, reasonPhrase);
    response.setHeader("Content-Type", contentType);
    for (size_t i = 0; i < extraHeaders.size(); ++i)
    {
        const std::string &name = extraHeaders[i].first;
        const std::string &value = extraHeaders[i].second;
        if (toLower(name) == "set-cookie")
            response.addHeader(name, value);
        else
            response.setHeader(name, value);
    }
    response.setBody(bodyPart);
    return response;
}



void CGI::handleInput()
{
    while (writeOffset < requestBody.size())
    {
        ssize_t written = write( pipeInFd,
            requestBody.data() + writeOffset,
            requestBody.size() - writeOffset
        );
        if (written > 0)
        {
            writeOffset += static_cast<size_t>(written);
            if (parentClient)
                parentClient->lastAction = std::time(NULL);

            continue;
        }
        if (written < 0 && errno == EINTR)
            continue;
        if (written < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            return;
        if (written < 0 && errno == EPIPE)
        {
            closeInput();
            return;
        }
        killCgi();
        return;
    }
    closeInput();
}

void CGI::handleOutput()
{
    char buffer[4096];
    while (true)
    {
        ssize_t bytesRead = read(pipeOutFd, buffer, sizeof(buffer));
        if (bytesRead > 0)
        {
            rawOutputBuffer.append(buffer, static_cast<size_t>(bytesRead));
            if (parentClient)
                parentClient->lastAction = std::time(NULL);
            continue;
        }
        if (bytesRead == 0)
        {
            closeOutput();
            return;
        }
        if (errno == EINTR)
            continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return;
        killCgi();
        return;
    }
}

void CGI::handleEvent(int fd, uint32_t events)
{
    if (state == DONE)
        return;

    if (fd == pipeInFd)
    {
        if (events & (EPOLLERR | EPOLLHUP))
            closeInput();
        else if (events & EPOLLOUT)
            handleInput();
    }
    else if (fd == pipeOutFd)
    {
        if (events & (EPOLLIN | EPOLLHUP))
            handleOutput();
        else if (events & EPOLLERR)
        {
            killCgi();
            return;
        }
    }
    if (state == DONE)
        return;
    if (pipeInFd == -1 && pipeOutFd == -1)
        finish();
}
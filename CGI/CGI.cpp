#include "CGI.hpp"
#include "../Server/Server.hpp"
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <cstring>
#include <cstdlib>
#include <sstream>
#include <errno.h>

char **CGI::buildEnv(const HttpRequest &request)
{
    std::string contentType;
    std::string contentLength;
    char **envp = new char*[5];
    
    if (request.hasHeader("content-type"))
    {
        const std::vector<std::string> &values = request.getHeader("content-type");
        if (!values.empty())
            contentType = values[0];
    }

    if (request.hasHeader("content-length"))
    {
        const std::vector<std::string> &values = request.getHeader("content-length");
        if (!values.empty())
            contentLength = values[0];
    }
    else
    {
        std::ostringstream oss;
        oss << request.getBody().size();
        contentLength = oss.str();
    }

    envp[0] = strdup(("REQUEST_METHOD=" + request.getMethod()).c_str());
    envp[1] = strdup(("QUERY_STRING=" + request.getQuery()).c_str());
    envp[2] = strdup(("CONTENT_TYPE=" + contentType).c_str());
    envp[3] = strdup(("CONTENT_LENGTH=" + contentLength).c_str());
    envp[4] = NULL;

    return envp;
}

void CGI::freeEnv(char **envp)
{
    if (!envp)
        return;
    for (int i = 0; envp[i]; i++)
        free(envp[i]);
    delete[] envp;
}

int CGI::getFD() const { return -1; }


CGI::CGI(Client* client, Server *srv, const HttpRequest &request, const Location &location, std::string fullResolvedPath)
    : pipeInFd(-1),
    pipeOutFd(-1),
    cgiPid(-1),
    writeOffset(0),
    state(WRITING_INPUT),
    server(srv),
    parentClient(client),
    execBin(location.cgiPath)
{
    if (!server || !parentClient)
        throw std::runtime_error("CGI: invalid server or client");

    parentClient->timeout = time(NULL);
    requestBody = request.getBody();
    char **envp = buildEnv(request);

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
            _exit(1);

        if (dup2(pipeOut[1], STDOUT_FILENO) < 0)
            _exit(1);

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
        _exit(1);
    }
    close(pipeIn[0]);
    close(pipeOut[1]);
    pipeInFd = pipeIn[1];
    pipeOutFd = pipeOut[0];
    freeEnv(envp);
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
                parentClient->timeout = time(NULL);

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
                parentClient->timeout = time(NULL);
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

HttpResponse CGI::parseCgiOutput(const std::string& rawOutput)
{
    int statusCode = 200;
    std::string reasonPhrase = "OK";
    std::string contentType = "text/html";

    size_t delimiter = rawOutput.find("\r\n\r\n");
    size_t delimiterLen = 4;

    if (delimiter == std::string::npos)
    {
        delimiter = rawOutput.find("\n\n");
        delimiterLen = 2;
    }
    
    if (delimiter == std::string::npos)
    {
        HttpResponse response(statusCode, reasonPhrase);
        response.setHeader("Content-Type", contentType);
    
        std::stringstream cl;
        cl << rawOutput.size();
        response.setHeader("Content-Length", cl.str());
        
        response.setBody(rawOutput);
        return response;
    }
    std::string headersPart = rawOutput.substr(0, delimiter);
    std::string bodyPart = rawOutput.substr(delimiter + delimiterLen);

    std::stringstream ss(headersPart);
    std::string line;

    while (std::getline(ss, line))
    {
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);
            
        if (line.find("Status:") == 0)
        {
            std::string statusValue = line.substr(7);
            std::stringstream statusStream(statusValue);

            statusStream >> statusCode;
            std::getline(statusStream >> std::ws, reasonPhrase);

            if (reasonPhrase.empty())
                reasonPhrase = "OK";
        }
        else if (line.find("Content-Type:") == 0)
        {
            contentType = line.substr(13);
            if (!contentType.empty() && contentType[0] == ' ')
                contentType.erase(0, 1);
        }
    }
    
    HttpResponse response(statusCode, reasonPhrase);
    response.setHeader("Content-Type", contentType);
    
    std::stringstream cl;
    cl << bodyPart.size();
    response.setHeader("Content-Length", cl.str());
    
    response.setBody(bodyPart);
    return response;
}


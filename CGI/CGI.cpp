#include "CGI.hpp"
#include "../Server/Server.hpp"
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <cstring>
#include <cstdlib>
#include <sstream>
#include <errno.h>

int CGI::getFD() const
{
    if (state == WRITING_INPUT)
        return pipeInFd;
    return pipeOutFd;
}

char **CGI::buildEnv(const HttpRequest &request)
{
    char** envp = new char*[5];
    envp[0] = strdup(("REQUEST_METHOD=" + request.getMethod()).c_str());
    envp[1] = strdup(("QUERY_STRING=" + request.getQuery()).c_str());
    envp[2] = strdup(("CONTENT_TYPE=" + request.getHeader("content-type")[0]).c_str());
    envp[3] = strdup((std::string("CONTENT_LENGTH=") + request.getHeader("content-length")[0]).c_str());
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

CGI::CGI(Client* client, Server *srv, const HttpRequest &request,
         const Location &location, std::string fullResolvedPath)
    : writeOffset(0),
      headersParsed(false),
      statusCode(200),
      statusReason("OK"),
      state(WRITING_INPUT),
      server(srv),
      parentClient(client)
{
    (void)location;
    client->timeout = time(NULL);
    requestBody = request.getBody();
    char **envp = buildEnv(request);

    int pipeIn[2], pipeOut[2];
    pipe(pipeIn);
    pipe(pipeOut);

    fcntl(pipeOut[0], F_SETFL, O_NONBLOCK);
    fcntl(pipeIn[1], F_SETFL, O_NONBLOCK);
    cgiPid = fork();
    if (cgiPid == 0)
    {
        dup2(pipeIn[0], 0);
        dup2(pipeOut[1], 1);
        close(pipeIn[1]);
        close(pipeOut[0]);
        close(pipeIn[0]);
        close(pipeOut[1]);
        char *args[] = { (char*)"/usr/bin/python3", (char*)fullResolvedPath.c_str(), NULL };
        execve(args[0], args, envp);
        
        exit(1);
    }
    close(pipeIn[0]);
    close(pipeOut[1]);

    pipeOutFd = pipeOut[0];
    pipeInFd = pipeIn[1];
    freeEnv(envp);
}

CGI::~CGI()
{
    if (pipeOutFd >= 0)
        close(pipeOutFd);
    if (pipeInFd >= 0)
        close(pipeInFd);    
    waitpid(cgiPid, NULL, WNOHANG);
}

void CGI::killCgi()
{
    if (cgiPid > 0)
    {
        kill(cgiPid, SIGKILL);
        cgiPid = -1;
    }
}

struct ParsedHeaders {
    int statusCode;
    std::string reasonPhrase;
    std::map<std::string, std::string> headers;
};

static ParsedHeaders parseHeadersOnly(const std::string& rawHeaders)
{
    ParsedHeaders result;
    result.statusCode = 200;
    result.reasonPhrase = "OK";

    std::istringstream stream(rawHeaders);
    std::string line;

    while (std::getline(stream, line)) {
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);

        if (line.empty())
            break;

        if (line.find("Status:") == 0) {
            std::string statusLine = line.substr(7);
            std::istringstream statusStream(statusLine);
            statusStream >> result.statusCode;
            std::getline(statusStream >> std::ws, result.reasonPhrase);
            if (result.reasonPhrase.empty())
                result.reasonPhrase = "OK";
        } else if (line.find("Content-Type:") == 0) {
            std::string ct = line.substr(13);
            if (!ct.empty() && ct[0] == ' ')
                ct.erase(0, 1);
            result.headers["Content-Type"] = ct;
        } else {
            size_t colon = line.find(':');
            if (colon != std::string::npos) {
                std::string key = trim(line.substr(0, colon));
                std::string value = trim(line.substr(colon + 1));
                result.headers[key] = value;
            }
        }
    }

    if (result.headers.find("Content-Type") == result.headers.end())
        result.headers["Content-Type"] = "text/html";

    return result;
}

void CGI::handleWrite()
{
    if (state != WRITING_INPUT)
        return;

    if (writeOffset >= requestBody.size())
    {
        server->removeHandler(pipeInFd);
        close(pipeInFd);
        state = READING_OUTPUT;
        server->addHandler(this, EPOLLIN); 
        return;
    }
    
    ssize_t written = write(pipeInFd, requestBody.c_str() + writeOffset, requestBody.size() - writeOffset);
    if (written > 0)
        writeOffset += written;

    if (writeOffset >= requestBody.size())
    {
        server->removeHandler(pipeInFd);
        close(pipeInFd);
        state = READING_OUTPUT;
        server->addHandler(this, EPOLLIN); 
    }
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

void CGI::handleRead()
{
    if (state == WRITING_INPUT)
        return;

    if (parentClient->getWriteBuffer().size() > cgi::MAX_BUFFER) {
        server->modifyHandler(this, 0);
        return;
    }

    char buffer[16384];
    while (true) {
        ssize_t bytesRead = read(pipeOutFd, buffer, sizeof(buffer));

        if (bytesRead > 0) {
            if (!headersParsed) {
                headerBuffer.append(buffer, bytesRead);
                size_t pos = headerBuffer.find("\r\n\r\n");
                if (pos != std::string::npos) {
                    ParsedHeaders parsed = parseHeadersOnly(headerBuffer.substr(0, pos));

                    std::ostringstream response;
                    response << "HTTP/1.1 " << parsed.statusCode << " " << parsed.reasonPhrase << "\r\n";
                    for (std::map<std::string, std::string>::const_iterator it = parsed.headers.begin();
                         it != parsed.headers.end(); ++it) {
                        response << it->first << ": " << it->second << "\r\n";
                    }
                    response << "\r\n";

                    parentClient->appendToWriteBuffer(response.str());
                    parentClient->setBodyAlreadyStreamed(true);

                    headersParsed = true;

                    std::string remaining = headerBuffer.substr(pos + 4);
                    if (!remaining.empty()) {
                        parentClient->appendToWriteBuffer(remaining);
                    }
                    headerBuffer.clear();
                }
            } else {
                parentClient->appendToWriteBuffer(std::string(buffer, bytesRead));
            }
        }
        else if (bytesRead == 0) {
            waitpid(cgiPid, NULL, WNOHANG);
            state = DONE;
            parentClient->onCgiDone(HttpResponse(200, "OK"));
            return;
        }
        else {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return;
            HttpResponse err(500, "Internal Server Error");
            state = DONE;
            parentClient->errorsHandler(500);
            return;
        }
    }
}
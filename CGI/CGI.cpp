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
    char** envp = new char*[3];
    envp[0] = strdup(("REQUEST_METHOD=" + request.getMethod()).c_str());
    envp[1] = strdup(("QUERY_STRING=" + request.getQuery()).c_str());
    envp[2] = NULL;
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
         const Location &location, std::string path)
    : writeOffset(0),
      state(WRITING_INPUT),
      server(srv),
      parentClient(client)
{
    (void)location;
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

        // 1. Clean the \r if it exists
        std::string cleanPath = path;
        if (!cleanPath.empty() && cleanPath[cleanPath.size() - 1] == '\r') {
            cleanPath.erase(cleanPath.size() - 1);
        }

        // 2. Remove the location prefix (e.g., "/cgi")
        if (cleanPath.find(location.path) == 0) {
            cleanPath.erase(0, location.path.length());
        }

        // 3. Build the final path
        std::string execPath = location.root + cleanPath;
        char *args[] = { (char*)"/usr/bin/python3", (char*)execPath.c_str(), NULL };
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
    
    // Fallback cleanup just in case the object is destroyed early
    waitpid(cgiPid, NULL, WNOHANG);
}

// Ensure you updated getFD() in your header/implementation as noted above!

void CGI::handleWrite()
{
    if (state != WRITING_INPUT)
        return;

    if (writeOffset >= requestBody.size())
    {
        server->removeHandler(pipeInFd, false);
        close(pipeInFd);
        // pipeInFd = -1;
        
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
        // pipeInFd = -1;
        
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
    response.setBody(bodyPart);

    return response;
}

void CGI::handleRead()
{
    if (state == WRITING_INPUT)
        return;

    char buffer[4096];
    while (true)
    {
        ssize_t bytesRead = read(pipeOutFd, buffer, sizeof(buffer));
        if (bytesRead > 0)
        {
            rawOutputBuffer.append(buffer, bytesRead);
        }
        else if (bytesRead == 0)
        {
            waitpid(cgiPid, NULL, 0);
            HttpResponse finalResponse = parseCgiOutput(rawOutputBuffer);

            state = DONE;
            // int fdToRemove = pipeOutFd;
            pipeOutFd = -1; 
            parentClient->onCgiDone(finalResponse);
            return; 
        }
        else
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return;

            HttpResponse err(500, "Internal Server Error");
            state = DONE;
            // int fdToRemove = pipeOutFd;
            pipeOutFd = -1;
            parentClient->onCgiDone(err);
            return;
        }
    }
}
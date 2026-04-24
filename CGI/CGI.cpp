#include "CGI.hpp" 
#include "../Server/Server.hpp" 
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <cstring>
#include <cstdlib>
#include <sstream>

CgiHandler::CgiHandler(Client* client, Server* srv, const HttpRequest& request, const Location& location, std::string path)
    : parentClient(client), server(srv)
{
    (void) location;
    
    char *my_envp[3];
    my_envp[2] = NULL;
    my_envp[0] = strdup(("REQUEST_METHOD=" + request.getMethod()).c_str());
    my_envp[1] = strdup(("QUERY_STRING=" + request.getTarget().substr(request.getTarget().find("?") + 1)).c_str());

    int pipeIn[2], pipeOut[2];
    pipe(pipeIn); 
    pipe(pipeOut);

    fcntl(pipeOut[0], F_SETFL, O_NONBLOCK);

    cgi_pid = fork();
    if (cgi_pid == 0) {
        dup2(pipeIn[0], 0);
        dup2(pipeOut[1], 1);
        
        char *cgi_filepath = strdup(path.insert(0, ".").c_str());
        char *args[] = {(char *) "/usr/bin/python3", cgi_filepath, NULL};
        
        execve(args[0], args, my_envp);
        exit(1);
    }

    close(pipeIn[0]);
    close(pipeOut[1]);
    
    pipe_fd = pipeOut[0];

    if (request.getMethod() == "POST") {
        write(pipeIn[1], request.getBody().c_str(), request.getBody().length());
    }
    close(pipeIn[1]);

    free(my_envp[0]);
    free(my_envp[1]);
}

CgiHandler::~CgiHandler()
{
    if (pipe_fd >= 0) close(pipe_fd);
    waitpid(cgi_pid, NULL, WNOHANG); 
}

int CgiHandler::getFD() const { return pipe_fd; }

void CgiHandler::handleWrite() {}

void CgiHandler::handleRead()
{
    char buffer[4096];
    int bytesRead = read(pipe_fd, buffer, sizeof(buffer));

    if (bytesRead > 0) 
    {
        rawOutputBuffer.append(buffer, bytesRead);
    }
    else if (bytesRead == 0) 
    {
        HttpResponse finalResponse = parseCgiOutput(rawOutputBuffer);
        parentClient->onCgiDone(finalResponse);
        server->removeHandler(pipe_fd); 
    }
    else 
    {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            HttpResponse errResponse(500, "Internal Server Error");
            parentClient->onCgiDone(errResponse);
            server->removeHandler(pipe_fd);
        }
    }
}

HttpResponse CgiHandler::parseCgiOutput(const std::string& rawOutput)
{
    int statusCode = 200;
    std::string statusReason = "OK";
    std::string contentType = "text/html";
    
    size_t delimiter = rawOutput.find("\r\n\r\n");
    size_t delimiterLen = 4;

    if (delimiter == std::string::npos) {
        delimiter = rawOutput.find("\n\n");
        delimiterLen = 2;
    }

    if (delimiter == std::string::npos) {
        HttpResponse response(statusCode, statusReason);
        response.setBody(rawOutput, contentType);
        return response;
    }

    std::string headersPart = rawOutput.substr(0, delimiter);
    std::string bodyPart = rawOutput.substr(delimiter + delimiterLen);

    std::stringstream ss(headersPart);
    std::string line;
    while (std::getline(ss, line)) 
    {
        if (!line.empty() && line[line.size() - 1] == '\r') line.erase(line.size() - 1);

        if (line.find("Content-Type: ") == 0) contentType = line.substr(14);
        else if (line.find("Status: ") == 0) {
            std::string statusValue = line.substr(8);
            std::stringstream statusStream(statusValue);
            statusStream >> statusCode;
            std::getline(statusStream >> std::ws, statusReason);
            if (statusReason.empty()) statusReason = "Custom Status";
        }
    }

    HttpResponse response(statusCode, statusReason);
    response.setBody(bodyPart, contentType);
    return response;
}
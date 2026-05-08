#include "Server.hpp"
#include "../CGI/CGI.hpp"
#include "../HTTP/HttpUtils/HttpUtils.hpp"
#include "../HTTP/HttpHandler.hpp"
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <iostream>
#include <cstdlib>
#include <ctime>

Client::Client(int fd, Server *srv, const std::vector<ServerConfig> &confs) 
    : socketFD(fd), server(srv), configs(confs), state(READING_REQUEST)
{
    timeout = time(NULL);
    activeCgi = NULL;   
    activeConfig = NULL; 
}

Client::~Client()
{
    if (socketFD >= 0)
    {
        ::close(socketFD);
        socketFD = -1;
    }
    if (activeCgi)
    {
        delete activeCgi;
        activeCgi = NULL;
    }
}

void Client::consumeReadBuffer(size_t bytes)
{
    if (bytes >= readBuffer.size())
        readBuffer.clear();
    else
        readBuffer.erase(0, bytes);
}

void Client::consumeWriteBuffer(size_t bytes)
{
    if (bytes >= writeBuffer.size())
        writeBuffer.clear();
    else
        writeBuffer.erase(0, bytes);
}

void Client::appendToWriteBuffer(const std::string& data) { writeBuffer += data; }
void Client::appendToReadBuffer(const char *data, size_t size) { readBuffer.append(data, size); }

int Client::getFD() const { return socketFD; }
HttpRequest &Client::getActiveRequest() { return activeRequest;}
bool Client::isConnected() const { return socketFD >= 0; }
bool Client::hasPendingWrite() const { return !writeBuffer.empty(); }
const std::string &Client::getReadBuffer() const { return readBuffer; }
const std::string &Client::getWriteBuffer() const { return writeBuffer; }

const ServerConfig *Client::matchConfig(const std::string& rawHost) const
{
    std::string host = rawHost;
    if (!host.empty() && host[host.size() - 1] == '\r') {
        host.erase(host.size() - 1);
    }
    size_t portSep = host.find(':');
    if (portSep != std::string::npos) 
        host = host.substr(0, portSep);
    
    host = toLower(host);

    for (size_t i = 0; i < configs.size(); ++i) 
    {
        for (size_t j = 0; j < configs[i].serverName.size(); ++j) 
        {
            if (toLower(configs[i].serverName[j]) == host) 
                return &configs[i];
        }
    }
    return &configs[0]; 
}

void Client::onCgiDone(HttpResponse response)
{
    appendToWriteBuffer(response.toString());
    activeRequest.reset();
    readBuffer.clear();
    state = SENDING_RESPONSE;
    
    if (activeCgi) 
    {
        server->removeHandler(activeCgi->getFD(), false); 
        delete activeCgi;
        activeCgi = NULL;
    }
    server->modifyHandler(this, EPOLLOUT);
}

void Client::errorsHandler(int errorCode)
{
    std::string reason = "Error";
    if (errorCode == 413) reason = "Payload Too Large";
    else if (errorCode == 400) reason = "Bad Request";
    else if (errorCode == 505) reason = "HTTP Version Not Supported";
    HttpResponse response = ErrorPage(errorCode, reason, configs[0]);
    response.setHeader("Connection", "close");
    appendToWriteBuffer(response.toString());
    state = SENDING_RESPONSE;
    server->modifyHandler(this, EPOLLOUT);
    activeRequest.reset();
    readBuffer.clear(); 
}

// const size_t ABSOLUTE_MAX_BUFFER = 10 * 1024 * 1024;

bool Client::readingFromSocket()
{
    char buf[8192];
    bool dataRead = false;

    while (true)
    {
        // if (readBuffer.size() > ABSOLUTE_MAX_BUFFER)
        //     break ;
        ssize_t bytes = recv(socketFD, buf, sizeof(buf), 0);
        if (bytes == 0)
        {
            server->removeHandler(socketFD);
            return false; 
        }
        if (bytes < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            server->removeHandler(socketFD);
            return false;
        }
        appendToReadBuffer(buf, static_cast<size_t>(bytes));
        dataRead = true;
        timeout = time(NULL);
    }
    return dataRead;
}

void Client::processRequestHeaders()
{
    const ServerConfig *selectedConfig = matchConfig(activeRequest.getHeader("host"));
    activeConfig = selectedConfig ? selectedConfig : &configs[0];
    activeRequest.setMaxBodySize(activeConfig->client_max_body_size);
    std::string cl = activeRequest.getHeader("content-length");
    if (!cl.empty() && myStold(cl) > activeConfig->client_max_body_size)
    {
        errorsHandler(413);
    }
}

void Client::executeRequest()
{
    HttpHandler handler(*activeConfig);
    HttpResult result = handler.process(activeRequest);
    if (result.type == HTTP_RESULT_CGI)
    {
        state = PROCESSING_CGI;
        activeCgi = new CGI(this, server, activeRequest, *result.cgiLocation, result.cgiRequestPath);
        server->addHandler(activeCgi, EPOLLOUT); 
    }
    else 
    {
        appendToWriteBuffer(result.response.toString());
        state = SENDING_RESPONSE;
    }
    consumeReadBuffer(activeRequest.getParsedSize());
    activeRequest.reset();
}

void Client::handleRead()
{
    if (state == PROCESSING_CGI || !readingFromSocket())
        return;
    std::cout << readBuffer << std::endl;
    while (true)
    {
        int parseStatus = activeRequest.parse(readBuffer);
        // readBuffer.erase(0, activeRequest.getParsedSize());
        if (activeRequest.getErrorCode() != 0) 
        {
            errorsHandler(activeRequest.getErrorCode());
            return;
        }
        if (parseStatus == 2)
        {
            processRequestHeaders();
            if (state == SENDING_RESPONSE) return; // Exit if 413 was triggered
            continue;
        }
        if (parseStatus == 0) break; 
        executeRequest();
        if (state == PROCESSING_CGI) 
            break;
    }
    if (hasPendingWrite() && state != PROCESSING_CGI)
    {
        if (state == SENDING_RESPONSE)
            server->modifyHandler(this, EPOLLOUT);
        else
            server->modifyHandler(this, EPOLLIN | EPOLLOUT);
    }
}

void Client::handleWrite()
{
    while (hasPendingWrite())
    {
        ssize_t bytes = send(socketFD, writeBuffer.c_str(), writeBuffer.size(), 0);
        if (bytes < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK) return;
            server->removeHandler(socketFD);
            return;
        }
        if (bytes == 0) return;       
        consumeWriteBuffer(static_cast<size_t>(bytes));
        timeout = time(NULL);
    }
    if (!hasPendingWrite())
    {
        if (state == SENDING_RESPONSE || activeRequest.getHeader("Connection") == "close")
        {
             server->removeHandler(socketFD);
             return;
        }
        state = READING_REQUEST;
        server->modifyHandler(this, EPOLLIN);
    }
}
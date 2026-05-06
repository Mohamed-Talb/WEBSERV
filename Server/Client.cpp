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
HttpRequest &Client::getRequest() { return request; }
bool Client::isConnected() const { return socketFD >= 0; }
bool Client::hasPendingWrite() const { return !writeBuffer.empty(); }
const std::string &Client::getReadBuffer() const { return readBuffer; }
const std::string &Client::getWriteBuffer() const { return writeBuffer; }

const ServerConfig *Client::matchConfig(const std::string& rawHost) const
{
    std::string host = rawHost;
    
    // FIXED: Strip the trailing \r before matching to prevent routing bugs
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
            if (configs[i].serverName[j] == host) 
                return &configs[i];
        }
    }
    return &configs[0]; 
}

void Client::onCgiDone(HttpResponse response)
{
    appendToWriteBuffer(response.toString());
    request.reset();
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

void Client::handleRead()
{
    if (state == PROCESSING_CGI)
        return ;
    char buf[8192];
    bool dataRead = false;

    while (true)
    {
        ssize_t bytes = recv(socketFD, buf, sizeof(buf), 0);
        if (bytes == 0)
        {
            server->removeHandler(socketFD);
            return;
        }
        if (bytes < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            server->removeHandler(socketFD);
            return;
        }
        appendToReadBuffer(buf, static_cast<size_t>(bytes));
        dataRead = true;
        timeout = time(NULL);
    }

    if (!dataRead)
        return;

    while (true)
    {
        int parseStatus = request.parse(readBuffer);

        if (request.getErrorCode() != 0)
        {
            HttpResponse response = ErrorPage(
                request.getErrorCode(),
                "Bad Request",
                configs[0]
            );

            appendToWriteBuffer(response.toString());
            state = SENDING_RESPONSE;
            server->modifyHandler(this, EPOLLIN | EPOLLOUT);

            request.reset();
            return;
        }

        if (request.getState() == PARSE_BODY || request.getState() == PARSE_COMPLETE)
        {
            const ServerConfig *selectedConfig = matchConfig(request.getHeader("host"));

            if (!selectedConfig)
                selectedConfig = &configs[0];

            std::string cl = request.getHeader("content-length");

            if (!cl.empty())
            {
                ssize_t bodySize = myStold(cl); // Ensure myStold handles potential trailing \r correctly in your util too

                if (bodySize > selectedConfig->client_max_body_size)
                {
                    HttpResponse err = ErrorPage(
                        413,
                        "Payload Too Large",
                        *selectedConfig
                    );

                    appendToWriteBuffer(err.toString());
                    state = SENDING_RESPONSE;
                    server->modifyHandler(this, EPOLLIN | EPOLLOUT);

                    request.reset();
                    readBuffer.clear();
                    return;
                }
            }
        }

        if (parseStatus == 0)
            break;

        if (parseStatus < 0)
            return;

        const ServerConfig *selectedConfig = matchConfig(request.getHeader("host"));

        if (!selectedConfig)
            selectedConfig = &configs[0];

        HttpHandler handler(*selectedConfig);
        HttpResult result = handler.process(request);
        
        if (result.type == HTTP_RESULT_CGI)
        {
            state = PROCESSING_CGI;
            CGI *cgi = new CGI(
                this,
                server,
                request,
                *result.cgiLocation,
                result.cgiRequestPath
            );
            activeCgi = cgi;
            server->addHandler(cgi, EPOLLOUT); 
            
            consumeReadBuffer(request.getParsedSize());
            request.reset();
        }
        else 
        {
            appendToWriteBuffer(result.response.toString());
            state = SENDING_RESPONSE;
            consumeReadBuffer(request.getParsedSize());
            request.reset();
        }
    }
    if (hasPendingWrite() && state != PROCESSING_CGI)
        server->modifyHandler(this, EPOLLIN | EPOLLOUT);
}

void Client::handleWrite()
{
    if (!hasPendingWrite())
        return;
        
    while (hasPendingWrite())
    {
        ssize_t bytes = send(socketFD, writeBuffer.c_str(), writeBuffer.size(), 0);
        if (bytes < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return;
            server->removeHandler(socketFD);
            return;
        }
        if (bytes == 0)
            return;
            
        consumeWriteBuffer(static_cast<size_t>(bytes));
        timeout = time(NULL);
    }
    
    if (!hasPendingWrite())
    {
        state = READING_REQUEST;
        server->modifyHandler(this, EPOLLIN);
    }
}
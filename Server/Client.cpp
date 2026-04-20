#include "Server.hpp"
#include "../HTTP/HttpHandler.hpp"
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>

Client::Client(int fd, Server* srv, const std::vector<ServerConfig>& confs) 
    : socketFD(fd), server(srv), configs(confs) {}

Client::~Client()
{
    closeConnection();
}

void Client::closeConnection()
{
    if (socketFD >= 0)
    {
        ::close(socketFD);
        socketFD = -1;
    }
}

bool Client::isConnected() const
{
    return socketFD >= 0;
}

void Client::appendToReadBuffer(const char* data, size_t size)
{
    readBuffer.append(data, size);
}

void Client::consumeReadBuffer(size_t bytes)
{
    if (bytes >= readBuffer.size())
        readBuffer.clear();
    else
        readBuffer.erase(0, bytes);
}

const std::string &Client::getReadBuffer() const
{
    return readBuffer;
}

void Client::clearReadBuffer()
{
    readBuffer.clear();
}

void Client::appendToWriteBuffer(const std::string& data)
{
    writeBuffer += data;
}

const std::string &Client::getWriteBuffer() const
{
    return writeBuffer;
}

void Client::consumeWriteBuffer(size_t bytes)
{
    writeBuffer.erase(0, bytes);
}

bool Client::hasPendingWrite() const
{
    return !writeBuffer.empty();
}

int Client::getFD() const
{
    return socketFD;
}

HttpRequest& Client::getRequest()
{
    return request;
}


const ServerConfig* Client::matchConfig(const std::string& rawHost) const
{
    std::string host = rawHost;
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

void Client::handleRead()
{
    char buf[8192];
    bool dataRead = false;
    while (true)
    {
        int bytes = recv(socketFD, buf, sizeof(buf), 0);
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
        appendToReadBuffer(buf, bytes);
        dataRead = true;
    }

    if (!dataRead)
        return;

    while (true)
    {
        int parseStatus = request.parse(readBuffer);
        if (parseStatus == 0)
            break;

        if (request.getErrorCode() != 0)
        {
            HttpHandler handler(configs[0]); 
            HttpResponse response = handler.process(request); // No second argument needed
            appendToWriteBuffer(response.toString());
            server->removeHandler(socketFD);
            return;
        }

        const ServerConfig *selectedConfig = matchConfig(request.getHeader("host"));
        if (!selectedConfig)
             selectedConfig = &configs[0];

        HttpHandler handler(*selectedConfig); 
        HttpResponse response = handler.process(request); // No second argument needed

        appendToWriteBuffer(response.toString());
        consumeReadBuffer(request.getConsumedBytes());
        request.reset(); 
    }

    if (hasPendingWrite())
    {
        server->modifyHandler(this, EPOLLIN | EPOLLOUT);
    }
}

void Client::handleWrite()
{
    if (!hasPendingWrite()) return;
    while (hasPendingWrite())
    {
        int bytes = send(socketFD, writeBuffer.c_str(), writeBuffer.size(), 0);
        if (bytes < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return;
            server->removeHandler(socketFD);
            return;
        }
        consumeWriteBuffer(bytes);
    }
    if (!hasPendingWrite())
    {
        server->modifyHandler(this, EPOLLIN);
    }
}
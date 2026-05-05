#include "Server.hpp"
#include "../CGI/CGI.hpp"
#include "../HTTP/HttpUtils/HttpUtils.hpp"
#include "../HTTP/HttpHandler.hpp"
#include "../Helpers.hpp"

#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
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

void Client::appendToWriteBuffer(const std::string &data)
{
    writeBuffer += data;
}

void Client::appendToReadBuffer(const char *data, size_t size)
{
    readBuffer.append(data, size);
}

int Client::getFD() const
{
    return socketFD;
}

HttpRequest &Client::getRequest()
{
    return request;
}

bool Client::isConnected() const
{
    return socketFD >= 0;
}

bool Client::hasPendingWrite() const
{
    return !writeBuffer.empty();
}

const std::string &Client::getReadBuffer() const
{
    return readBuffer;
}

const std::string &Client::getWriteBuffer() const
{
    return writeBuffer;
}

const ServerConfig *Client::matchConfig(const std::string &rawHost) const
{
    if (configs.empty())
        return NULL;

    std::string host = rawHost;

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

void Client::onCgiDone(const HttpResponse &response)
{
    appendToWriteBuffer(response.toString());
    state = SENDING_RESPONSE;

    server->modifyHandler(socketFD, this, EPOLLIN | EPOLLOUT);
}

void Client::handleParseError()
{
    const ServerConfig *selectedConfig = matchConfig(request.getHeader("host"));

    if (!selectedConfig)
        selectedConfig = &configs[0];

    HttpHandler handler(*selectedConfig);
    HttpResult result = handler.process(request);

    appendToWriteBuffer(result.response.toString());
    state = SENDING_RESPONSE;

    server->modifyHandler(socketFD, this, EPOLLIN | EPOLLOUT);

    request.reset();
}

bool Client::processParsedRequest()
{
    const ServerConfig *selectedConfig = matchConfig(request.getHeader("host"));

    if (!selectedConfig)
        selectedConfig = &configs[0];

    HttpHandler handler(*selectedConfig);
    HttpResult result = handler.process(request);

    // if (result.type == HTTP_RESULT_CGI)
    // {
    //     state = PROCESSING_CGI;

    //     CgiHandler *cgi = new CgiHandler(
    //         this,
    //         server,
    //         request,
    //         *result.cgiLocation,
    //         result.cgiRequestPath
    //     );

    //     server->addHandler(cgi->getFD(), cgi, EPOLLIN);

    //     consumeReadBuffer(request.getParsedSize());
    //     request.reset();

    //     return false;
    // }
    appendToWriteBuffer(result.response.toString());
    state = SENDING_RESPONSE;

    consumeReadBuffer(request.getParsedSize());
    request.reset();
    return true;
}

void Client::enableWriteIfNeeded()
{
    if (hasPendingWrite() && state != PROCESSING_CGI)
        server->modifyHandler(socketFD, this, EPOLLIN | EPOLLOUT);
}

void Client::handleParseError()
{
    HttpResponse response = ErrorPage(
        request.getErrorCode(),
        "Bad Request",
        configs[0]
    );

    appendToWriteBuffer(response.toString());
    state = SENDING_RESPONSE;

    server->modifyHandler(socketFD, this, EPOLLIN | EPOLLOUT);

    request.reset();
}

int Client::readFromSocket()
{
    char buf[8192];
    bool dataRead = false;

    while (true)
    {
        ssize_t bytes = recv(socketFD, buf, sizeof(buf), 0);
        if (bytes == 0)
        {
            server->removeHandler(socketFD);
            return -1;
        }
        if (bytes < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            server->removeHandler(socketFD);
            return -1;
        }
        appendToReadBuffer(buf, static_cast<size_t>(bytes));
        dataRead = true;
        timeout = time(NULL);
    }
    if (!dataRead)
        return 0;
    return 1;
}

void Client::handleRead(int fd)
{
    if (fd != socketFD)
        return;
    if (state == PROCESSING_CGI)
        return;
    int readStatus = readFromSocket();
    if (readStatus <= 0)
        return;
    while (true)
    {
        int parseStatus = request.parse(readBuffer);
        if (request.getErrorCode() != 0)
        {
            handleParseError();
            return;
        }
        if (parseStatus == 0)
            break;
        if (parseStatus < 0)
            return;
        if (!processParsedRequest())
            break;
    }
    enableWriteIfNeeded();
}

void Client::handleWrite(int fd)
{
    if (fd != socketFD)
        return;
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
        server->modifyHandler(socketFD, this, EPOLLIN);
    }
}
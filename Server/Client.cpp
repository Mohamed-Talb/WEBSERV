#include "./Server.hpp"
#include "../CGI/CGI.hpp"
#include "../HTTP/HttpUtils/HttpUtils.hpp"
#include "../HTTP/HttpHandler.hpp"

#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <ctime>

Client::Client(int fd, Server *srv, const std::vector<ServerConfig> &confs)
    : socketFD(fd),
      server(srv),
      configs(confs),
      activeCgi(NULL),
      activeConfig(NULL),
      state(READING_REQUEST),
      closeAfterWrite(false),
      timeout(time(NULL))
{
}

Client::~Client()
{
    if (socketFD >= 0)
    {
        ::close(socketFD);
        socketFD = -1;
    }

    activeCgi = NULL;
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

int Client::getFD() const
{
    return socketFD;
}

void Client::appendToWriteBuffer(const std::string &data)
{
    writeBuffer += data;
}

void Client::appendToReadBuffer(const char *data, size_t size)
{
    readBuffer.append(data, size);
}

HttpRequest &Client::getActiveRequest()
{
    return activeRequest;
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

    if (!host.empty() && host[host.size() - 1] == '\r')
        host.erase(host.size() - 1);

    size_t portSeparator = host.find(':');

    if (portSeparator != std::string::npos)
        host = host.substr(0, portSeparator);

    host = toLower(host);

    for (size_t i = 0; i < configs.size(); ++i)
    {
        for (size_t j = 0; j < configs[i].serverNames.size(); ++j)
        {
            if (toLower(configs[i].serverNames[j]) == host)
                return &configs[i];
        }
    }

    return &configs[0];
}

void Client::closeConnection()
{
    if (activeCgi)
    {
        CGI *cgi = activeCgi;
        activeCgi = NULL;

        cgi->killCgi();
        server->removeHandler(cgi->getFD());
    }

    server->removeHandler(socketFD);
}

void Client::terminateCgi()
{
    if (!activeCgi)
        return;

    activeCgi->killCgi();

    const ServerConfig *config = activeConfig;

    if (!config && !configs.empty())
        config = &configs[0];

    if (!config)
    {
        closeConnection();
        return;
    }

    HttpResponse response = ErrorPage(504, *config);

    response.setHeader("Connection", "close");
    closeAfterWrite = true;

    if (activeCgi)
    {
        int cgiFD = activeCgi->getFD();

        activeCgi = NULL;
        server->removeHandler(cgiFD);
    }

    writeBuffer = response.toString();

    activeRequest.reset();
    readBuffer.clear();

    state = SENDING_RESPONSE;
    server->modifyHandler(this, EPOLLOUT);
}

void Client::onCgiDone(HttpResponse response)
{
    closeAfterWrite = activeRequest.shouldCloseConnection();

    if (closeAfterWrite)
        response.setHeader("Connection", "close");

    if (activeCgi)
    {
        int cgiFD = activeCgi->getFD();

        activeCgi = NULL;
        server->removeHandler(cgiFD);
    }

    writeBuffer = response.toString();

    activeRequest.reset();

    state = SENDING_RESPONSE;
    server->modifyHandler(this, EPOLLOUT);
}

void Client::errorsHandler(int errorCode)
{
    const ServerConfig *config = activeConfig;

    if (!config && !configs.empty())
        config = &configs[0];

    if (!config)
    {
        closeConnection();
        return;
    }

    HttpResponse response = ErrorPage(errorCode, *config);

    response.setHeader("Connection", "close");
    closeAfterWrite = true;

    writeBuffer = response.toString();

    activeRequest.reset();
    readBuffer.clear();

    state = SENDING_RESPONSE;
    server->modifyHandler(this, EPOLLOUT);
}

void Client::handleRead()
{
    if (state != READING_REQUEST)
        return;

    char buffer[8192];
    bool receivedData = false;

    while (true)
    {
        ssize_t bytes = recv(socketFD, buffer, sizeof(buffer), 0);

        if (bytes > 0)
        {
            appendToReadBuffer(buffer, static_cast<size_t>(bytes));
            receivedData = true;
            timeout = time(NULL);
            continue;
        }

        if (bytes == 0)
        {
            closeConnection();
            return;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK)
            break;

        closeConnection();
        return;
    }

    if (!receivedData)
        return;

    while (state == READING_REQUEST)
    {
        ParseStatus parseStatus = activeRequest.parse(readBuffer);

        if (activeRequest.getErrorCode() != 0)
        {
            errorsHandler(activeRequest.getErrorCode());
            return;
        }

        switch (parseStatus)
        {
            case PARSE_HEADERS_COMPLETE:
            {
                activeConfig = matchConfig(activeRequest.getHeader("host"));

                if (!activeConfig)
                {
                    closeConnection();
                    return;
                }

                activeRequest.setMaxBodySize(activeConfig->client_max_body_size);

                std::string contentLength = activeRequest.getHeader("content-length");

                if (!contentLength.empty()
                    && myStold(contentLength) > activeConfig->client_max_body_size)
                {
                    errorsHandler(413);
                    return;
                }

                continue;
            }

            case PARSE_NEED_MORE_DATA:
            {
                size_t consumedBytes = activeRequest.getParsedSize();

                if (consumedBytes > 0)
                {
                    consumeReadBuffer(consumedBytes);
                    activeRequest.setParsedSize(0);
                }
                return;
            }

            case PARSE_REQUEST_COMPLETE:
            {
                if (!activeConfig)
                {
                    if (configs.empty())
                    {
                        closeConnection();
                        return;
                    }

                    activeConfig = &configs[0];
                }

                HttpHandler handler(*activeConfig);
                HttpResult result = handler.process(activeRequest);

                size_t consumedBytes = activeRequest.getParsedSize();
                closeAfterWrite = activeRequest.shouldCloseConnection();

                consumeReadBuffer(consumedBytes);

                if (result.type == HTTP_RESULT_CGI)
                {
                    state = PROCESSING_CGI;

                    activeCgi = new CGI(this, server, activeRequest,
                        *result.cgiLocation, result.cgiRequestPath);

                    server->addHandler(activeCgi, EPOLLOUT);
                    return;
                }

                HttpResponse response = result.response;

                if (closeAfterWrite)
                    response.setHeader("Connection", "close");

                writeBuffer = response.toString();

                activeRequest.reset();

                state = SENDING_RESPONSE;
                server->modifyHandler(this, EPOLLOUT);
                return;
            }
        }
    }
}

void Client::handleWrite()
{
    while (hasPendingWrite())
    {
        ssize_t bytes = send(socketFD, writeBuffer.c_str(), writeBuffer.size(), 0);

        if (bytes > 0)
        {
            consumeWriteBuffer(static_cast<size_t>(bytes));
            timeout = time(NULL);
            continue;
        }

        if (bytes == 0)
            return;

        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return;

        closeConnection();
        return;
    }

    if (closeAfterWrite)
    {
        closeConnection();
        return;
    }

    closeAfterWrite = false;
    activeConfig = NULL;
    state = READING_REQUEST;

    server->modifyHandler(this, EPOLLIN);
}
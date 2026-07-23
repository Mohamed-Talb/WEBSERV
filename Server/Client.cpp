#include "./Server.hpp"
#include "../CGI/CGI.hpp"
#include "../HTTP/HttpUtils/HttpUtils.hpp"
#include "../HTTP/HttpHandler.hpp"
#include "../HTTP/HttpRequestParser.hpp"

#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <ctime>

Client::Client(int fd, Server *srv, const std::vector<ServerConfig *> confs)
    : socketFD(fd),
      server(srv),
      configs(confs),
      activeConfig(NULL),
      requestParser(),
      activeCgi(NULL),
      state(READING_REQUEST),
      closeAfterWrite(false),
      writeOffset(0),
      timeout(time(NULL)) {}

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

ClientState Client::getState() const { return state;}

// void Client::consumeWriteBuffer(size_t bytes)
// {
//     if (bytes >= writeBuffer.size())
//         writeBuffer.clear();
//     else
//         writeBuffer.erase(0, bytes);
// }

int  Client::getFD() const { return socketFD;}
bool Client::isConnected() const { return socketFD >= 0;}
bool Client::hasPendingWrite() const { return !writeBuffer.empty();}
const std::string &Client::getReadBuffer() const { return readBuffer;}
const std::string &Client::getWriteBuffer() const { return writeBuffer;}
HttpRequest &Client::getActiveRequest() { return requestParser.getRequest();}

void Client::appendToWriteBuffer(const std::string &data) { writeBuffer += data;}
void Client::appendToReadBuffer(const char *data, size_t size) { readBuffer.append(data, size);}

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
        for (size_t j = 0; j < configs[i]->serverNames.size(); ++j)
        {
            if (toLower(configs[i]->serverNames[j]) == host)
                return configs[i];
        }
    }
    return configs[0];
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
        config = configs[0];

    if (!config)
    {
        closeConnection();
        return;
    }

    HttpResponse response = ErrorPage(504, *config);

    response.setHeader("Connection", "close");
    closeAfterWrite = true;

    int cgiFD = activeCgi->getFD();

    activeCgi = NULL;
    server->removeHandler(cgiFD);

    writeBuffer = response.toString();

    requestParser.reset();
    readBuffer.clear();

    state = SENDING_RESPONSE;
    server->modifyHandler(this, EPOLLOUT);
}

void Client::onCgiDone(HttpResponse response)
{
    HttpRequest &request = requestParser.getRequest();

    closeAfterWrite = request.shouldCloseConnection();

    if (closeAfterWrite)
        response.setHeader("Connection", "close");

    if (activeCgi)
    {
        int cgiFD = activeCgi->getFD();

        activeCgi = NULL;
        server->removeHandler(cgiFD);
    }

    writeBuffer = response.toString();

    requestParser.reset();

    state = SENDING_RESPONSE;
    server->modifyHandler(this, EPOLLOUT);
}

void Client::errorsHandler(int errorCode)
{
    const ServerConfig *config = activeConfig;

    if (!config && !configs.empty())
        config = configs[0];

    if (!config)
    {
        closeConnection();
        return;
    }

    HttpResponse response = ErrorPage(errorCode, *config);

    response.setHeader("Connection", "close");
    closeAfterWrite = true;

    writeBuffer = response.toString();

    requestParser.reset();
    readBuffer.clear();

    state = SENDING_RESPONSE;
    server->modifyHandler(this, EPOLLOUT);
}

bool Client::readFromSocket()
{
    char buffer[8192];

    while (true)
    {
        ssize_t bytes = recv(socketFD,buffer,sizeof(buffer),0);
        if (bytes > 0)
        {
            appendToReadBuffer(buffer, static_cast<size_t>(bytes));
            timeout = time(NULL);
            continue;
        }
        if (bytes == 0)
        {
            closeConnection();
            return false;
        }
        if (errno == EINTR)
            continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return true;
        closeConnection();
        return false;
    }
}

void Client::processReadBuffer()
{
 while (state == READING_REQUEST)
    {
        ParseStatus parseStatus = requestParser.parse(readBuffer);
        HttpRequest &request = requestParser.getRequest();

        if (parseStatus == PARSE_REQUEST_ERROR)
        {
            errorsHandler(requestParser.getErrorCode());
            return;
        }
        switch (parseStatus)
        {
            case PARSE_HEADERS_COMPLETE:
            {
                const std::vector<std::string> &hostValues = request.getHeader("host");
                activeConfig = matchConfig(hostValues[0]);
                if (!configs.empty())
                    activeConfig = configs[0];
                if (!activeConfig)
                {
                    closeConnection();
                    return;
                }
                const Location *location = matchLocation(*activeConfig,request.getRequestPath());
                size_t maxBodySize = location->client_max_body_size;
                requestParser.setMaxBodySize(maxBodySize);
                continue;
            }
            case PARSE_NEED_MORE_DATA:
                return;
            case PARSE_REQUEST_COMPLETE:
            {
                if (!activeConfig)
                {
                    if (configs.empty())
                    {
                        closeConnection();
                        return;
                    }
                    activeConfig = configs[0];
                }

                HttpHandler handler(*activeConfig);
                HttpResult result = handler.process(request);

                size_t consumedBytes = requestParser.getParsedSize();
                closeAfterWrite = request.shouldCloseConnection();

                consumeReadBuffer(consumedBytes);
                if (result.type == HTTP_RESULT_CGI)
                {
                    state = PROCESSING_CGI;
                    activeCgi = new CGI(this, server, request, *result.cgiLocation, result.cgiRequestPath);
                    try
                    {
                        server->addHandler(activeCgi, EPOLLOUT);
                    }
                    catch (...)
                    {
                        delete activeCgi;
                        throw ;
                    }
                    activeCgi = activeCgi;
                    state = PROCESSING_CGI;
                }
                HttpResponse response = result.response;

                if (closeAfterWrite)
                    response.setHeader("Connection", "close");

                writeBuffer = response.toString();
                requestParser.reset();

                state = SENDING_RESPONSE;
                server->modifyHandler(this, EPOLLOUT);
                return ;
            }
            case PARSE_REQUEST_ERROR:
                errorsHandler(requestParser.getErrorCode());
                return;
        }
    }
}

void Client::handleRead()
{
    if (state != READING_REQUEST)
        return;
    if (!readFromSocket())
        return;
    processReadBuffer();
}


void Client::handleWrite()
{
    while (writeOffset < writeBuffer.size())
    {
        ssize_t bytesSent = send(socketFD,  writeBuffer.data() + writeOffset, writeBuffer.size() - writeOffset,MSG_NOSIGNAL);
        if (bytesSent > 0)
        {
            writeOffset += static_cast<size_t>(bytesSent);
            continue;
        }
        if (bytesSent == 0)
            return;
        if (errno == EINTR)
            continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return;
        closeConnection();
        return;
    }
    if (writeOffset == writeBuffer.size())
    {
        writeBuffer.clear();
        writeOffset = 0;
    }
    if (closeAfterWrite)
    {
        closeConnection();
        return;
    }
    closeAfterWrite = false;
    activeConfig = NULL;
    state = READING_REQUEST;

    if (!readBuffer.empty())
    {
        processReadBuffer();
        if (state == READING_REQUEST)
            server->modifyHandler(this, EPOLLIN);
        return;
    }
    server->modifyHandler(this, EPOLLIN);
}
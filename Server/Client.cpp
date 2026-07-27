#include "./Server.hpp"
#include "../CGI/CGI.hpp"
#include "../HTTP/HttpUtils/HttpUtils.hpp"
#include "../HTTP/HttpHandler.hpp"
#include "../HTTP/HttpRequestParser.hpp"

#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <ctime>

Client::Client(int fd, Server *srv, const std::vector<ServerConfig *> &confs)
    :socketFD(fd), server(srv), activeCgi(NULL), state(READING_REQUEST),writeOffset(0),  closeAfterWrite(false),requestParser(confs),activeConfig(NULL), configs(confs),  timeout(time(NULL)) {}

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

int  Client::getFD() const { return socketFD;}
bool Client::isConnected() const { return socketFD >= 0;}
bool Client::hasPendingWrite() const { return !writeBuffer.empty();}
const std::string &Client::getReadBuffer() const { return readBuffer;}
const std::string &Client::getWriteBuffer() const { return writeBuffer;}
HttpRequest &Client::getActiveRequest() { return requestParser.getRequest();}

void Client::appendToWriteBuffer(const std::string &data) { writeBuffer += data;}
void Client::appendToReadBuffer(const char *data, size_t size) { readBuffer.append(data, size);}

void Client::closeConnection()
{
    if (activeCgi)
    {
        CGI *cgi = activeCgi;
        activeCgi = NULL;
        cgi->killCgi();
    }
    writeOffset = 0;
    timeout = time(NULL);
    server->removeHandler(this);
}

void Client::terminateCgi()
{
    if (!activeCgi) 
        return;
    const ServerConfig *config = activeConfig ? activeConfig : configs[0];
    if (!config) 
    {
        closeConnection();
        return;
    }
    HttpResponse response = ErrorPage(504, *config);
    response.setHeader("Connection", "close");
    closeAfterWrite = true;

    delete activeCgi;
    activeCgi = NULL;

    writeBuffer = response.toString();
    readBuffer.clear();
    requestParser.reset();
    state = SENDING_RESPONSE;
    server->modifyHandler(socketFD, EPOLLOUT);
}

void Client::onCgiDone(HttpResponse &response)
{
    std::cout << response.getBodySize() << std::endl;
    HttpRequest &request = requestParser.getRequest();

    closeAfterWrite = request.shouldCloseConnection();
    if (closeAfterWrite)
        response.setHeader("Connection", "close");

    activeCgi = NULL;
    writeBuffer = response.toString();
    writeOffset = 0;
    timeout = time(NULL);
    requestParser.reset();
    state = SENDING_RESPONSE;
    server->modifyHandler(socketFD, EPOLLOUT);
}

void Client::startCgi(const HttpRequest &request, const HttpResult &result)
{
    if (!result.cgiLocation)
    {
        errorsHandler(500);
        return;
    }
    Session *session = NULL;
    bool isNewSession = false;
    std::string sessionId;
    if (request.getCookie("session_id", sessionId))
        session = server->getSessionManager().findSession(sessionId);
    if (!session)
    {
        session = server->getSessionManager().createSession();
        if (!session)
        {
            errorsHandler(500);
            return;
        }
        isNewSession = true;
    }
    state = PROCESSING_CGI;
    try
    {
        activeCgi = new CGI( this, server, request, *result.cgiLocation, result.cgiRequestPath, session->getId(), isNewSession);
        activeCgi->registerHandlers();
    }
    catch (...)
    {
        if (activeCgi)
        {
            CGI *cgi = activeCgi;
            activeCgi = NULL;
            cgi->killCgi();
        }
        if (isNewSession)
            server->getSessionManager().removeSession(session->getId());

        state = READING_REQUEST;
        errorsHandler(500);
        return;
    }
    return;
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
    writeOffset = 0;
    timeout = time(NULL);
    requestParser.reset();
    readBuffer.clear();
    state = SENDING_RESPONSE;
    server->modifyHandler(socketFD, EPOLLOUT);
}



bool Client::readFromSocket()
{
    char buffer[65536];
    size_t totalRead = 0;
    const size_t maxReadPerCall = 1024 * 1024;

    while (totalRead < maxReadPerCall)
    {
        ssize_t bytes = recv(socketFD, buffer, sizeof(buffer), 0);
        if (bytes > 0)
        {
            appendToReadBuffer(buffer, static_cast<size_t>(bytes));
            totalRead += static_cast<size_t>(bytes);
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
    return true;
}

void Client::processReadBuffer()
{
    while (state == READING_REQUEST)
    {
        ParseStatus parseStatus = requestParser.parse(readBuffer);

        if (parseStatus == PARSE_NEED_MORE_DATA)
            return;
        if (parseStatus == PARSE_REQUEST_ERROR)
        {
            activeConfig = requestParser.getActiveConfig();
            errorsHandler(requestParser.getErrorCode());
            return;
        }
        if (parseStatus == PARSE_REQUEST_COMPLETE)
        {
            HttpRequest &request = requestParser.getRequest();
            std::cout << "[CLIENT REQUEST BODY]: " << request.getBody().size() << std::endl;
            activeConfig = requestParser.getActiveConfig();

            if (!activeConfig)
            {
                errorsHandler(500);
                return;
            }
            size_t consumedBytes = requestParser.getParsedSize();
            closeAfterWrite = request.shouldCloseConnection();

            HttpHandler handler(*activeConfig);
            HttpResult result = handler.process(request);

            consumeReadBuffer(consumedBytes);
            if (result.type == HTTP_RESULT_CGI)
            {
                startCgi(request, result);
                return;
            }
            HttpResponse response = result.response;
            if (closeAfterWrite)
                response.setHeader("Connection", "close");
            writeBuffer = response.toString();
            writeOffset = 0;
            requestParser.reset();
            state = SENDING_RESPONSE;
            server->modifyHandler(socketFD, EPOLLOUT);
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
            server->modifyHandler(socketFD, EPOLLIN);
        return;
    }
    server->modifyHandler(socketFD, EPOLLIN);
}

void Client::handleEvent(int fd, uint32_t events)
{
    if (fd == socketFD)
    {
        if (events & EPOLLIN)
            handleRead();
        if (events & EPOLLOUT)
            handleWrite();
    }
}
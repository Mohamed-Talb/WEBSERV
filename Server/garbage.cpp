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



#include "Listener.hpp"

#include <cerrno>
#include <cstring>

Listener::Listener(const std::vector<ServerConfig *> &confs, Server *srv) : socketFD(-1), server(srv), configs(confs)
{
    if (!server)
        throw std::runtime_error("LISTENER: invalid server");

    if (configs.empty() || !configs[0])
        throw std::runtime_error("LISTENER: no server configuration");

    ServerConfig *conf = configs[0];
    socketFD = socket(AF_INET, SOCK_STREAM, 0);

    if (socketFD < 0)
        throw std::runtime_error("LISTENER: socket() failed on port " + intToString(conf->port));

    int opt = 1;

    if (setsockopt(socketFD, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        close(socketFD);
        socketFD = -1;
        throw std::runtime_error("LISTENER: setsockopt() failed on port " + intToString(conf->port));
    }

    int flags = fcntl(socketFD, F_GETFL, 0);
    if (flags < 0 || fcntl(socketFD, F_SETFL, flags | O_NONBLOCK) < 0)
    {
        close(socketFD);
        socketFD = -1;
        throw std::runtime_error("LISTENER: fcntl() failed on port " + intToString(conf->port));
    }

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(conf->port);
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_addr.s_addr = inet_addr(conf->host.c_str());

    if (addr.sin_addr.s_addr == INADDR_NONE)
        addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(socketFD, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0)
    {
        close(socketFD);
        socketFD = -1;
        throw std::runtime_error("LISTENER: bind() failed on port " + intToString(conf->port));
    }
    if (listen(socketFD, SOMAXCONN) < 0)
    {
        close(socketFD);
        socketFD = -1;
        throw std::runtime_error("LISTENER: listen() failed on port " + intToString(conf->port));
    }
}

Listener::~Listener()
{
    if (socketFD >= 0)
    {
        close(socketFD);
        socketFD = -1;
    }
}

void Listener::handleEvent(int fd, uint32_t events)
{
    if (fd != socketFD)
        return;
    if (events & (EPOLLERR | EPOLLHUP))
    {
        server->removeHandler(this);
        return;
    }
    if (!(events & EPOLLIN))
        return;
    while (true)
    {
        int clientFD = accept(socketFD, NULL, NULL);
        std::cout << "[CONNECTION]: new client in fd = " << clientFD << std::endl;
        if (clientFD >= 0)
        {
            int flags = fcntl(clientFD, F_GETFL, 0);
            if (flags < 0 || fcntl(clientFD, F_SETFL, flags | O_NONBLOCK) < 0)
            {
                close(clientFD);
                continue;
            }
            Client *newClient = NULL;
            try
            {
                newClient = new Client(clientFD, server, configs);
                server->addHandler(newClient, clientFD, EPOLLIN);
            }
            catch (...)
            {
                delete newClient;
                throw;
            }
            continue;
        }
        if (errno == EINTR)
            continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return;
        return;
    }
}

int Listener::getFD() const
{
    return socketFD;
}

int Listener::getPort() const
{
    if (configs.empty() || !configs[0])
        return -1;
    return configs[0]->port;
}




#include "Server.hpp"
#include <sys/epoll.h>
#include <unistd.h>
#include <stdexcept>

Server::Server() : epollFD(-1), lastSessionCleanup(time(NULL)), sessionManager() {}

SessionManager &Server::getSessionManager()
{
    return sessionManager;
}

const std::vector<ServerConfig>& Server::getConfigs() const
{
    return configs;
}

void Server::clearDeletionQueue()
{
    std::set<IEventHandler *> pending = deletionQueue;
    deletionQueue.clear();

    for (std::set<IEventHandler *>::iterator it = pending.begin(); it != pending.end(); ++it)
        delete *it;
}


Server::~Server()
{
    std::map<int, IEventHandler*>::iterator it;
    for (it = fdHandlers.begin(); it != fdHandlers.end(); ++it)
    {
        delete it->second;
    }
    fdHandlers.clear();
    if (epollFD >= 0)
    {
        ::close(epollFD);
        epollFD = -1;
    }
}

void Server::init(const std::vector<ServerConfig> &confs)
{
    configs = confs;
    epollFD = epoll_create(1000);
    if (epollFD < 0)
        throw std::runtime_error("SERVER: epoll_create failed");
    
    std::map<std::string, std::vector<ServerConfig *> > groupedConfigs;
    for (size_t i = 0; i < configs.size(); ++i) 
    {
        std::string ipPort = configs[i].host + ":" + intToString(configs[i].port);
        groupedConfigs[ipPort].push_back(&configs[i]);
    }
    std::map<std::string, std::vector<ServerConfig *> >::iterator it;
    for (it = groupedConfigs.begin(); it != groupedConfigs.end(); ++it)
    {
        Listener* listener = new Listener(it->second, this); 
        addHandler(listener, listener->getFD(), EPOLLIN);
    }
}


void Server::unregisterFD(int fd)
{
    std::map<int, IEventHandler *>::iterator fdIt = fdHandlers.find(fd);

    if (fdIt == fdHandlers.end())
        return;

    IEventHandler *handler = fdIt->second;
    epoll_ctl(epollFD, EPOLL_CTL_DEL, fd, NULL);
    fdHandlers.erase(fdIt);

    std::map<IEventHandler *, std::set<int> >::iterator handlerIt = registeredFds.find(handler);
    if (handlerIt == registeredFds.end())
        return;

    handlerIt->second.erase(fd);
    if (handlerIt->second.empty())
        registeredFds.erase(handlerIt);
}


void Server::addHandler(IEventHandler *handler, int fd, uint32_t events)
{
    if (!handler)
        throw std::runtime_error("SERVER: null handler");

    if (fd < 0)
        throw std::runtime_error("SERVER: invalid file descriptor");

    if (fdHandlers.find(fd) != fdHandlers.end())
        throw std::runtime_error("SERVER: file descriptor already registered");

    epoll_event event;
    std::memset(&event, 0, sizeof(event));
    event.events = events;
    event.data.fd = fd;

    if (epoll_ctl(epollFD, EPOLL_CTL_ADD, fd, &event) < 0)
        throw std::runtime_error("SERVER: epoll_ctl ADD failed");

    fdHandlers[fd] = handler;
    registeredFds[handler].insert(fd);
}

void Server::modifyHandler(int fd, uint32_t events)
{
    if (fdHandlers.find(fd) == fdHandlers.end())
        return;

    epoll_event event;
    std::memset(&event, 0, sizeof(event));
    event.events = events;
    event.data.fd = fd;

    if (epoll_ctl(epollFD, EPOLL_CTL_MOD, fd, &event) < 0)
        throw std::runtime_error("SERVER: epoll_ctl MOD failed");
}

void Server::removeHandler(IEventHandler *handler)
{
    if (!handler)
        return;

    std::map<IEventHandler *, std::set<int> >::iterator handlerIt = registeredFds.find(handler);
    if (handlerIt != registeredFds.end())
    {
        std::set<int> fds = handlerIt->second;
        for (std::set<int>::iterator it = fds.begin(); it != fds.end(); ++it)
        {
            int fd = *it;
            epoll_ctl(epollFD, EPOLL_CTL_DEL, fd, NULL);
            fdHandlers.erase(fd);
        }
        registeredFds.erase(handlerIt);
    }
    deletionQueue.insert(handler);
}


void Server::checkTimeout()
{
    time_t currentTime = time(NULL);
    std::vector<Client *> expiredClients;
    std::vector<Client *> cgiTimeoutClients;

    for (std::map<IEventHandler *, std::set<int> >::iterator it = registeredFds.begin(); it != registeredFds.end(); ++it)
    {
        Client *client = dynamic_cast<Client *>(it->first);

        if (!client)
            continue;

        if (difftime(currentTime, client->timeout) <= TIMEOUT_DURATION)
            continue;

        if (client->getState() == PROCESSING_CGI)
            cgiTimeoutClients.push_back(client);
        else
            expiredClients.push_back(client);
    }

    for (size_t i = 0; i < expiredClients.size(); ++i)
        removeHandler(expiredClients[i]);

    for (size_t i = 0; i < cgiTimeoutClients.size(); ++i)
        cgiTimeoutClients[i]->terminateCgi();

    if (difftime(currentTime, lastSessionCleanup) >= SESSION_CLEANUP_INTERVAL)
    {
        sessionManager.removeExpiredSessions(currentTime, SESSION_TIMEOUT);
        lastSessionCleanup = currentTime;
    }
}


void Server::eventLoop()
{
    const int MAX_EVENTS = 1024;
    epoll_event readyEvents[MAX_EVENTS];

    while (true)
    {
        this->clearDeletionQueue();
        int ready = epoll_wait(epollFD, readyEvents, MAX_EVENTS, 1000);
        if (ready == -1)
        {
            if (errno == EINTR) // os just wanted us to check a signal
                continue;
            break; // To Do: unrecoverable error, clean up resources and inform the user using perror
        }
        for (int i = 0; i < ready; ++i)
        {
            int fd = readyEvents[i].data.fd;
            uint32_t currEvent = readyEvents[i].events;

            if (fdHandlers.find(fd) == fdHandlers.end())
                continue;
            IEventHandler* handler = fdHandlers[fd];
            if (currEvent & (EPOLLERR | EPOLLHUP))
            {
                currEvent |= EPOLLIN | EPOLLOUT;
            }
            handler->handleEvent(fd, currEvent);
        }
        checkTimeout();
    }
    // cleanup funtion needed here for when we break off the loop
}

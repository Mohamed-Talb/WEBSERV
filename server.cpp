#include "Client.hpp"
#include "Server.hpp"
#include "Listener.hpp"


Client::Client(int fd, Server *srv, const std::vector<ServerConfig *> &confs)
    : socketFD(fd), 
    server(srv),
    activeCgi(NULL),
    state(READING_REQUEST),
    writeOffset(0),
    closeAfterWrite(false),
    requestParser(confs),
    activeConfig(NULL),
    configs(confs),
    lastAction(std::time(NULL)) {}

Client::~Client()
{
    if (socketFD >= 0)
    {
        ::close(socketFD);
        socketFD = -1;
    }
    if (activeCgi)
    {
        CGI *cgi = activeCgi;
        activeCgi = NULL;
        cgi->killCgi();
    }
}

void Client::consumeReadBuffer(size_t bytes)
{
    if (bytes >= readBuffer.size())
    {
        std::string().swap(readBuffer);
        return;
    }
    std::string remaining = readBuffer.substr(bytes);
    remaining.swap(readBuffer);
}


int  Client::getFD() const { return socketFD;}
ClientState Client::getState() const { return state;}
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
    server->removeHandler(this);
}

void Client::terminateCgi()
{
    if (!activeCgi)
        return;

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

    CGI *cgi = activeCgi;
    activeCgi = NULL;
    cgi->killCgi();

    writeBuffer = response.toString();
    writeOffset = 0;
    lastAction = std::time(NULL);

    requestParser.reset();
    readBuffer.clear();

    state = SENDING_RESPONSE;
    server->modifyHandler(socketFD, EPOLLOUT);
}

void Client::onCgiDone(HttpResponse &response)
{
    HttpRequest &request = requestParser.getRequest();

    closeAfterWrite = request.shouldCloseConnection();

    if (closeAfterWrite)
        response.setHeader("Connection", "close");
    else if (request.getVersion() == "HTTP/1.0")
        response.setHeader("Connection", "keep-alive");

    activeCgi = NULL;

    writeBuffer = response.toString();
    writeOffset = 0;
    lastAction = std::time(NULL);

    requestParser.reset();

    state = SENDING_RESPONSE;
    server->modifyHandler(socketFD, EPOLLOUT);
}


void Client::startCgi(HttpRequest &request, const HttpResult &result)
{
    if (!result.cgiLocation)
    {
        errorsHandler(500);
        return;
    }
    state = PROCESSING_CGI;
    try
    {
        activeCgi = new CGI(this, server, request, 
                           result.cgiRequestPath, result.cgiInterpreter, activeConfig);
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
        errorsHandler(500);
    }
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
    lastAction = std::time(NULL);

    requestParser.reset();
    readBuffer.clear();

    state = SENDING_RESPONSE;
    server->modifyHandler(socketFD, EPOLLOUT);
}

bool Client::readFromSocket()
{
    char buffer[65536];

    ssize_t bytesRead = recv(socketFD, buffer, sizeof(buffer), 0);
    if (bytesRead > 0)
    {
        appendToReadBuffer(buffer, static_cast<size_t>(bytesRead));
        lastAction = std::time(NULL);
        return true;
    }
    closeConnection();
    return false;
}

void Client::processReadBuffer()
{
    while (state == READING_REQUEST)
    {
        ParseStatus status = requestParser.parse(readBuffer);
        size_t consumed = requestParser.getParsedSize();
        if (consumed > 0)
        {
            consumeReadBuffer(consumed);
            requestParser.resetParsedSize();
        }
        if (status == PARSE_NEED_MORE_DATA)
            return;
        if (status == PARSE_REQUEST_ERROR)
        {
            activeConfig = requestParser.getActiveConfig();
            errorsHandler(requestParser.getErrorCode());
            return;
        }
        if (status != PARSE_REQUEST_COMPLETE)
        {
            errorsHandler(500);
            return;
        }
        HttpRequest &request = requestParser.getRequest();
        activeConfig = requestParser.getActiveConfig();
        if (!activeConfig)
        {
            errorsHandler(500);
            return;
        }
        closeAfterWrite = request.shouldCloseConnection();
        HttpHandler handler(*activeConfig);
        HttpResult result = handler.process(request);
        if (result.type == HTTP_RESULT_CGI)
        {
            startCgi(request, result);
            return;
        }
        HttpResponse response = result.response;
        if (closeAfterWrite)
            response.setHeader("Connection", "close");
        else if (request.getVersion() == "HTTP/1.0")
            response.setHeader("Connection", "keep-alive");
        writeBuffer = response.toString();
        writeOffset = 0;

        requestParser.reset();
        state = SENDING_RESPONSE;
        server->modifyHandler(socketFD, EPOLLOUT);
        return;
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
        ssize_t bytesSent = send(
            socketFD,
            writeBuffer.data() + writeOffset,
            writeBuffer.size() - writeOffset,
            MSG_NOSIGNAL
        );
        if (bytesSent > 0)
        {
            writeOffset += static_cast<size_t>(bytesSent);
            lastAction = std::time(NULL);
            continue;
        }
        if (bytesSent == 0)
            return;
        return;
    }

    writeBuffer.clear();
    writeOffset = 0;
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
        if (state == SENDING_RESPONSE)
            server->modifyHandler(socketFD, EPOLLOUT);
        else if (state == READING_REQUEST)
            server->modifyHandler(socketFD, EPOLLIN);
        return;
    }
    server->modifyHandler(socketFD, EPOLLIN);
}

void Client::handleEvent(int fd, uint32_t events)
{
    if (fd != socketFD)
        return;
    if (events & EPOLLIN)
    {
        handleRead();
    }
    if (events & EPOLLOUT)
    {
        handleWrite();
    }
    if (events & (EPOLLERR | EPOLLHUP))
    {
        closeConnection();
        return;
    }
}


#include "Server.hpp"
#include "Client.hpp"
#include "Listener.hpp"


Listener::Listener(const std::vector<ServerConfig *> &confs, Server *srv) : socketFD(-1), server(srv), configs(confs)
{
    if (configs.empty() || !configs[0])
        throw std::runtime_error("LISTENER: missing configuration");

    ServerConfig *conf = configs[0];
    socketFD = socket(AF_INET, SOCK_STREAM, 0);

    if (socketFD < 0)
        throw std::runtime_error("LISTENER: create socket() failed");

    int opt = 1;
    if (setsockopt(socketFD, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        close(socketFD);
        socketFD = -1;
        throw std::runtime_error("LISTENER: setsockopt() failed on port " + intToString(conf->port));
    }

    int flags = fcntl(socketFD, F_GETFL);
    if (flags < 0 || fcntl(socketFD, F_SETFL, flags | O_NONBLOCK) < 0)
    {
        close(socketFD);
        socketFD = -1;
        throw std::runtime_error("LISTENER: fcntl() failed on port " + intToString(conf->port));
    }

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(conf->port);
    addr.sin_addr.s_addr = inet_addr(conf->host.c_str());
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
        if (clientFD < 0)
        {
            if (errno == EINTR)
                continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return;
            server->removeHandler(this);
            return;
        }
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
            std::cout << "[CONNECTION]: new Client in FD = " << clientFD << std::endl;
        }
        catch (...)
        {
            if (newClient)
                delete newClient;
            else
                close(clientFD);

            continue;
        }
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
#include "Client.hpp"
#include "Listener.hpp"

Server::Server() : epollFD(-1) {}


Server::~Server()
{
    clearDeletionQueue();
    HandlerFDMap::iterator it;
    for (it = handlers.begin(); it != handlers.end(); ++it)
        delete it->first;

    handlers.clear();
    fdHandlers.clear();
    if (epollFD >= 0)
    {
        close(epollFD);
        epollFD = -1;
    }
}

void Server::init(const std::vector<ServerConfig> &confs)
{
    configs = confs;
    epollFD = epoll_create(1000);
    if (epollFD < 0)
        throw std::runtime_error("SERVER: epoll_create failed");

    std::map<int, bool> wildcardPorts;
    for (size_t i = 0; i < configs.size(); ++i)
    {
        if (configs[i].host == "0.0.0.0")
            wildcardPorts[configs[i].port] = true;
    }
    ServerConfigMap groupedConfigs;
    for (size_t i = 0; i < configs.size(); ++i)
    {
        std::string listenerHost = configs[i].host;
        if (wildcardPorts[configs[i].port])
            listenerHost = "0.0.0.0";
        
        std::string key = listenerHost + ":" + intToString(configs[i].port);
        groupedConfigs[key].push_back(&configs[i]);
    }
    for (ServerConfigMap::iterator it = groupedConfigs.begin(); it != groupedConfigs.end(); ++it)
    {
        Listener *listener = NULL;
        try
        {
            listener = new Listener(it->second, this);
            addHandler(listener, listener->getFD(), EPOLLIN);
        }
        catch (...)
        {
            delete listener;
            throw;
        }
    }
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
    event.data.fd = fd;
    event.events = events;

    if (epoll_ctl(epollFD, EPOLL_CTL_ADD, fd, &event) < 0)
        throw std::runtime_error("SERVER: epoll_ctl ADD failed");

    fdHandlers[fd] = handler;
    handlers[handler].insert(fd);
}

void Server::modifyHandler(int fd, uint32_t events)
{
    epoll_event event;
    std::memset(&event, 0, sizeof(event));
    event.data.fd = fd;
    event.events = events;

    if (epoll_ctl(epollFD, EPOLL_CTL_MOD, fd, &event) < 0)
        throw std::runtime_error("SERVER: epoll_ctl MOD failed");
}

void Server::unregisterFD(int fd)
{
    FDHandlerMap::iterator fdIt = fdHandlers.find(fd);
    if (fdIt == fdHandlers.end())
        return;

    IEventHandler *handler = fdIt->second;
    fdHandlers.erase(fdIt);
    epoll_ctl(epollFD, EPOLL_CTL_DEL, fd, NULL);

    HandlerFDMap::iterator it = handlers.find(handler);
    if (it == handlers.end())
        return;

    it->second.erase(fd);
    if (it->second.empty())
        handlers.erase(it);
}

void Server::removeHandler(IEventHandler *handler)
{
    if (!handler)
        return;

    HandlerFDMap::iterator handlerIt = handlers.find(handler);
    if (handlerIt != handlers.end())
    {
        std::set<int> fds = handlerIt->second;
        for (std::set<int>::iterator it = fds.begin(); it != fds.end(); ++it)
        {
            int fd = *it;
            epoll_ctl(epollFD, EPOLL_CTL_DEL, fd, NULL);
            fdHandlers.erase(fd);
        }
        handlers.erase(handlerIt);
    }
    deletionQueue.insert(handler);
}

const std::vector<ServerConfig>& Server::getConfigs() const
{
    return configs;
}

void Server::checkTimeout()
{
    time_t currentTime = std::time(NULL);
    std::vector<Client *> expiredClients;
    std::vector<Client *> cgiTimeoutClients;

    for (HandlerFDMap::iterator it = handlers.begin(); it != handlers.end(); ++it)
    {
        Client *client = dynamic_cast<Client *>(it->first);
        if (!client)
            continue;

        if (difftime(currentTime, client->lastAction) <= TIMEOUT_DURATION)
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
}

void Server::clearDeletionQueue()
{
    std::set<IEventHandler *> pending = deletionQueue;
    deletionQueue.clear();

    for (std::set<IEventHandler *>::iterator it = pending.begin(); it != pending.end(); ++it)
        delete *it;
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
            if (errno == EINTR)
                continue;
            break;
        }
        for (int i = 0; i < ready; ++i)
        {
            int fd = readyEvents[i].data.fd;
            uint32_t currEvent = readyEvents[i].events;

            if (fdHandlers.find(fd) == fdHandlers.end())
                continue;
            IEventHandler* handler = fdHandlers[fd];
            handler->handleEvent(fd, currEvent);
        }
        checkTimeout();
    }
}

#include "Server.hpp"

Client::Client(int fd) : socketFD(fd) {}

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

void Client::appendToReadBuffer(const std::string& data)
{
    readBuffer += data;
}

const std::string& Client::getReadBuffer() const
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

const std::string& Client::getWriteBuffer() const
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

int Client::getSocketFD() const
{
    return socketFD;
}


// ─── Listener.cpp ───────────────────────────────────────────────────
#include "Server.hpp"
#include "Client.hpp"
#include <cctype>
#include <fstream>

Listener::Listener(const std::vector<ServerConfig>& confs): socketFD(-1), configs(confs)
{
    loadListener(configs[0]); 
}

Listener::~Listener()
{
    closeListener();
}

void Listener::loadListener(const ServerConfig& conf)
{
    socketFD = socket(AF_INET, SOCK_STREAM, 0);
    if (socketFD < 0)
        throw ServerException("Listener","socket() failed on port " + intToString(conf.port));
    int opt = 1;
    if (setsockopt(socketFD, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        ::close(socketFD);
        socketFD = -1;
        throw ServerException("Listener",
            "setsockopt() failed on port " + intToString(conf.port));
    }

    sockaddr_in addr;
    // std::memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(conf.port);
    addr.sin_addr.s_addr = inet_addr(conf.host.c_str()); 

    if (addr.sin_addr.s_addr == INADDR_NONE)
        addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(socketFD, (sockaddr*)&addr, sizeof(addr)) < 0)
    {
        ::close(socketFD);
        socketFD = -1;
        throw ServerException("Listener","bind() failed on port " + intToString(conf.port));
    }

    if (listen(socketFD, SOMAXCONN) < 0)
    {
        ::close(socketFD);
        socketFD = -1;
        throw ServerException("Listener","listen() failed on port " + intToString(conf.port));
    }
    std::cout << "[LISTENER] : Successfully bound and listening on Port " 
              << conf.port << " (Socket FD: " << socketFD << ")" << std::endl;
}

void Listener::closeListener()
{
    for (std::map<int, Client*>::iterator it = clients.begin(); it != clients.end(); ++it)
    {
        it->second->closeConnection();
        delete it->second;
    }
    clients.clear();
    if (socketFD >= 0)
    {
        ::close(socketFD);
        socketFD = -1;
    }
}



// const ServerConfig& Listener::matchConfig(const std::string& requestedHost) const // ????????????????
// {
//     return ;
// }
const ServerConfig& Listener::matchConfig(const std::string& hostHeader) const
{
    std::string requestedHost = hostHeader;
    size_t portSep = requestedHost.find(':');
    if (portSep != std::string::npos)
        requestedHost = requestedHost.substr(0, portSep);
    for (size_t i = 0; i < configs.size(); ++i)
    {
        for (size_t j = 0; j < configs[i].serverName.size(); ++j)
        {
            if (configs[i].serverName[j] == requestedHost)
                return configs[i];
        }
    }
    return configs[0];
}




Client* Listener::acceptClient()
{
    int clientFD = accept(socketFD, NULL, NULL);
    if (clientFD < 0)
        return NULL; // EAGAIN/EWOULDBLOCK — no client ready, not an error
    if (fcntl(clientFD, F_SETFL, O_NONBLOCK) < 0)
    {
        ::close(clientFD);
        throw ServerException("Listener","fcntl() failed for new client on port " + intToString(configs[0].port));
    }
    Client* client       = new Client(clientFD);
    clients[clientFD]    = client;
    return client;
}

Client* Listener::getClient(int fd) const
{
    std::map<int, Client*>::const_iterator it = clients.find(fd);
    if (it != clients.end())
        return it->second;
    return NULL;
}

void Listener::removeClient(int clientFD)
{
    std::map<int, Client*>::iterator it = clients.find(clientFD);
    if (it == clients.end())
        return;
    it->second->closeConnection();
    delete it->second;
    clients.erase(it);
}

int Listener::getSocketFD() const { return socketFD; }
int Listener::getPort()     const { return configs[0].port; }

IOState Listener::handleClientRead(Client* client)
{
    char buf[4096];
    int clientFD = client->getSocketFD();
    int bytes = recv(clientFD, buf, sizeof(buf), 0);

    if (bytes == 0)
        return IO_DISCONNECTED;
    if (bytes < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return IO_PENDING;
        return IO_DISCONNECTED;
    }

    client->appendToReadBuffer(std::string(buf, bytes));
    std::string readBuffer = client->getReadBuffer();

    HttpRequest request;
    int parseStatus = request.parse(readBuffer);

    if (parseStatus == 0)
        return IO_PENDING;

    HttpResponse response;
    HttpHandler handler;

    if (request.getErrorCode() != 0)
    {
        response = handler.process(request, configs[0]); 
    }
    else
    {
        std::cout << "[LISTENER] : Client FD " << clientFD
                  << " fully received request: " << request.getMethod() << " " << request.getTarget() << std::endl;
        
        std::string host = request.getHeader("host");
        const ServerConfig& selectedConfig = matchConfig(host);
        response = handler.process(request, selectedConfig);
    }

    client->appendToWriteBuffer(response.toString());
    size_t parsedBytes = request.getConsumedBytes();
    client->clearReadBuffer();
    if (readBuffer.size() > parsedBytes)
    {
        client->appendToReadBuffer(readBuffer.substr(parsedBytes));
    }

    return IO_READY;
}

IOState Listener::handleClientWrite(Client* client)
{
    if (!client->hasPendingWrite())
        return IO_READY;
    int clientFD = client->getSocketFD();
    const std::string& buffer = client->getWriteBuffer();
    int bytes = send(clientFD, buffer.c_str(), buffer.size(), 0);

    if (bytes < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return IO_PENDING;
        return IO_DISCONNECTED;
    }
    client->consumeWriteBuffer(bytes);
    if (!client->hasPendingWrite())
    {
        // Note form mtaleb: For HTTP/1.1 Keep-Alive, you go back to reading.  For HTTP/1.0 Connection: close, you might actually return -1 here.
        return IO_READY; 
    }
    return IO_PENDING;
}



#include "Server.hpp"
// ─── Server.cpp ─────────────────────────────────────────────────────
Server::Server() : epollFD(-1) {}

Server::~Server() { shutdown(); }

void Server::registerToEpoll(int fd, uint32_t events)
{
    epoll_event ev;
    ev.events  = events;
    ev.data.fd = fd;
    if (epoll_ctl(epollFD, EPOLL_CTL_ADD, fd, &ev) < 0)
    {
        ::close(epollFD);
        epollFD = -1;
        throw ServerException("Server", "epoll_ctl ADD failed for fd " + intToString(fd));
    }
}

void Server::init(std::vector<ServerConfig>& configs)
{
    std::cout << "[SERVER] : Initializing server... Grouping configs." << std::endl;
    epollFD = epoll_create(1024);
    if (epollFD < 0)
        throw ServerException("Server", "epoll_create() failed");

    std::map<int, std::vector<ServerConfig> > groupedConfigs;
    for (size_t i = 0; i < configs.size(); ++i) 
    {
        groupedConfigs[configs[i].port].push_back(configs[i]);
    }
    for (std::map<int, std::vector<ServerConfig> >::iterator it = groupedConfigs.begin(); it != groupedConfigs.end(); ++it)
    {
        Listener* listener = new Listener(it->second); 
        listeners[listener->getSocketFD()] = listener;
        registerToEpoll(listener->getSocketFD(), EPOLLIN);
    }
    std::cout << "[SERVER] : Epoll created with FD " << epollFD << std::endl;
}

void Server::shutdown()
{
    for (map<int, Listener*>::iterator it = listeners.begin(); it != listeners.end(); ++it)
    {
        delete it->second;
    }
    listeners.clear();
    if (epollFD >= 0)
    {
        ::close(epollFD);
        epollFD = -1;
    }
}



void Server::disconnectClient(Listener* listener, int clientFD)
{
    epoll_ctl(epollFD, EPOLL_CTL_DEL, clientFD, NULL); 
    listener->removeClient(clientFD);
    clientMap.erase(clientFD);
    std::cout << "[SERVER] : Disconnecting Client FD " << clientFD << std::endl;
}


void Server::handleNewConnection(Listener* listener)
{
    Client* client = listener->acceptClient();
    if (!client)
        return;
    int clientFD = client->getSocketFD();
    clientMap[clientFD] = listener;
    epoll_event ev;
    ev.events  = EPOLLIN;
    ev.data.fd = clientFD;
    if (epoll_ctl(epollFD, EPOLL_CTL_ADD, clientFD, &ev) < 0)
    {
        logError("handleNewConnection", "epoll_ctl ADD failed for client fd " + intToString(clientFD));
        listener->removeClient(clientFD);
        clientMap.erase(clientFD); // Clean up if epoll fails
        return;
    }
    std::cout << "[SERVER] : New connection! Assigned Client FD " << clientFD 
            << " on port " << listener->getPort() << std::endl;
}

void Server::handleConnection(Listener *listener, int clientFd, uint32_t event)
{
    Client* client = listener->getClient(clientFd);
    if (event & (EPOLLHUP | EPOLLERR))
    {
        disconnectClient(listener, clientFd);
        return ;
    }
    if (event & EPOLLIN)
    {
        IOState status = listener->handleClientRead(client);
        if (status == IO_DISCONNECTED) 
        {
            disconnectClient(listener, clientFd);
            return ;
        }
        if (status == IO_READY)
        {
            epoll_event ev;
            ev.events  = EPOLLIN | EPOLLOUT;
            ev.data.fd = clientFd;
            epoll_ctl(epollFD, EPOLL_CTL_MOD, clientFd, &ev);
        }
        // If status == IO_PENDING, we do nothing and let the loop continue
    }
    if (event & EPOLLOUT)
    {
        IOState status = listener->handleClientWrite(client);
        if (status == IO_DISCONNECTED)
        {
            disconnectClient(listener, clientFd);
            return;
        }
        if (status == IO_READY) 
        {
            epoll_event ev;
            ev.events  = EPOLLIN;
            ev.data.fd = clientFd;
            epoll_ctl(epollFD, EPOLL_CTL_MOD, clientFd, &ev);
        }
    }   
}

void Server::runEventLoop()
{
    const int MAX_EVENTS = 1024;
    epoll_event readyEvents[MAX_EVENTS];
    

    while (true)
    {
        int ready = epoll_wait(epollFD, readyEvents, MAX_EVENTS, -1); // handling eppoll wait errors 

        for (int i = 0; i < ready; ++i)
        {
            int fd = readyEvents[i].data.fd;
            uint32_t currEvent = readyEvents[i].events;

            std::map<int, Listener*>::iterator listenerIt = listeners.find(fd);
            if (listenerIt != listeners.end())
            {
                handleNewConnection(listenerIt->second);
                continue;
            
            }
            std::map<int, Listener*>::iterator clientIt = clientMap.find(fd);
            if (clientIt == clientMap.end()) 
                continue;
            Listener* parentListener = clientIt->second;
            handleConnection(parentListener, fd, currEvent);
        }
    }
}





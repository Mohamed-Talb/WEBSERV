#ifndef SERVER_HPP
#define SERVER_HPP

#include <map>
#include <vector>
#include "../Lib.hpp"
#include "Server.ipp"
#include "../Helpers.hpp"
#include "../HTTP/HttpRequest.hpp"
#include "../configParser/configParser.hpp"
#include "../Errors.hpp"

class Listener;

class Server
{
    private:
    int epollFD;
    std::vector<ServerConfig> configs;
    std::map<int, IEventHandler*> fdHandlers;

    public:
    Server();
    ~Server();

    void runEventLoop();
    void removeHandler(int fd);
    void init(const std::vector<ServerConfig>& confs);
    void addHandler(IEventHandler* handler, uint32_t events);
    void modifyHandler(IEventHandler* handler, uint32_t events);

    const std::vector<ServerConfig>& getConfigs() const;
};


class Client : public IEventHandler
{
    private:
    HttpRequest request;
    int         socketFD;
    std::string readBuffer;
    std::string writeBuffer;
    
    Server* server;
    std::vector<ServerConfig> configs;

    Client();
    Client(const Client&);
    const ServerConfig* matchConfig(const std::string& host) const;

    public:
    virtual ~Client();
    Client(int fd, Server* srv, const std::vector<ServerConfig>& confs);

    void closeConnection();
    bool isConnected() const;

    void clearReadBuffer();
    bool hasPendingWrite() const;
    void consumeReadBuffer(size_t bytes);
    void consumeWriteBuffer(size_t bytes);
    void appendToWriteBuffer(const std::string& data);
    void appendToReadBuffer(const char* data, size_t size);
    
    virtual void handleRead();
    virtual void handleWrite();

    HttpRequest &getRequest();
    const std::string &getReadBuffer() const;
    const std::string &getWriteBuffer() const;

    virtual int  getFD() const;
};


class Listener : public IEventHandler 
{
        private:
        int                       socketFD;
        std::vector<ServerConfig> configs;
        Server* server; 

        void closeListener(); 
        void loadListener(const ServerConfig &conf);
        
        Listener();
        Listener(const Listener&);
        Listener& operator=(const Listener&);

        public:
        virtual ~Listener();
        Listener(const std::vector<ServerConfig>& confs, Server* srv);
        
        virtual void handleRead();
        virtual void handleWrite();

        int getPort() const;
        virtual int  getFD() const;
};

#endif




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


const ServerConfig* Client::matchConfig(const std::string& host) const
{
    for (size_t i = 0; i < configs.size(); ++i) 
    {
        for (size_t j = 0; j < configs[i].serverName.size(); ++j) 
        {
            if (configs[i].serverName[j] == host) 
            {
                return &configs[i]; // Found an exact match!
            }
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

    if (!dataRead) return;

    while (true)
    {
        int parseStatus = request.parse(readBuffer);
        if (parseStatus == 0)
            break;
        HttpResponse response;
        HttpHandler handler;

        if (request.getErrorCode() != 0)
        {
            response = handler.process(request, configs[0]); 
        }
        else
        {
            std::string host = request.getHeader("host");
            size_t portSep = host.find(':');
            if (portSep != std::string::npos) 
                host = host.substr(0, portSep);
            const ServerConfig* selectedConfig = matchConfig(host);
            response = handler.process(request, *selectedConfig);
        }
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



#include "Server.hpp" 
#include <cctype>
#include <fstream>

Listener::Listener(const std::vector<ServerConfig>& confs, Server* srv)
    : socketFD(-1), configs(confs), server(srv)
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
        throw ServerException("Listener", "socket() failed on port " + intToString(conf.port));

    int opt = 1;
    if (setsockopt(socketFD, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        ::close(socketFD);
        socketFD = -1;
        throw ServerException("Listener", "setsockopt() failed on port " + intToString(conf.port));
    }
    if (fcntl(socketFD, F_SETFL, O_NONBLOCK) < 0)
    {
        ::close(socketFD);
        socketFD = -1;
        throw ServerException("Listener", "fcntl() failed on port " + intToString(conf.port));
    }
    sockaddr_in addr;
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(conf.port);
    addr.sin_addr.s_addr = inet_addr(conf.host.c_str()); 

    if (addr.sin_addr.s_addr == INADDR_NONE)
        addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(socketFD, (sockaddr*)&addr, sizeof(addr)) < 0)
    {
        ::close(socketFD);
        socketFD = -1;
        throw ServerException("Listener", "bind() failed on port " + intToString(conf.port));
    }
    if (listen(socketFD, SOMAXCONN) < 0)
    {
        ::close(socketFD);
        socketFD = -1;
        throw ServerException("Listener", "listen() failed on port " + intToString(conf.port));
    }
}

void Listener::closeListener()
{
    if (socketFD >= 0)
    {
        ::close(socketFD);
        socketFD = -1;
    }
}

void Listener::handleRead()
{
    while (true)
    {
        int clientFD = accept(socketFD, NULL, NULL);
        
        if (clientFD < 0)
        {
            break; 
        }
        
        if (fcntl(clientFD, F_SETFL, O_NONBLOCK) < 0)
        {
            ::close(clientFD);
            continue;
        }
        Client* newClient = new Client(clientFD, server, configs);
        server->addHandler(newClient, EPOLLIN); 
    }
}

void Listener::handleWrite()
{
}

int Listener::getFD() const
{
    return socketFD;
}

int Listener::getPort() const 
{ 
    return configs[0].port; 
}




#include "Server.hpp"
#include <sys/epoll.h>
#include <unistd.h>
#include <stdexcept>

Server::Server() : epollFD(-1) {}

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

void Server::init(const std::vector<ServerConfig>& confs)
{
    configs = confs;
    epollFD = epoll_create(1024);
    if (epollFD < 0)
        throw ServerException("Server", "epoll_create failed");
    std::map<int, std::vector<ServerConfig> > groupedConfigs;
    for (size_t i = 0; i < configs.size(); ++i) 
    {
        groupedConfigs[configs[i].port].push_back(configs[i]);
    }
    std::map<int, std::vector<ServerConfig> >::iterator it;
    for (it = groupedConfigs.begin(); it != groupedConfigs.end(); ++it)
    {
        Listener* listener = new Listener(it->second, this); 
        addHandler(listener, EPOLLIN);
    }
}

void Server::addHandler(IEventHandler* handler, uint32_t events)
{
    int fd = handler->getFD();
    fdHandlers[fd] = handler;
    
    epoll_event ev;
    ev.events = events;
    ev.data.fd = fd;
    epoll_ctl(epollFD, EPOLL_CTL_ADD, fd, &ev);
}

void Server::modifyHandler(IEventHandler* handler, uint32_t events)
{
    epoll_event ev;
    ev.events = events;
    ev.data.fd = handler->getFD();
    epoll_ctl(epollFD, EPOLL_CTL_MOD, handler->getFD(), &ev);
}

void Server::removeHandler(int fd)
{
    epoll_ctl(epollFD, EPOLL_CTL_DEL, fd, NULL);
    std::map<int, IEventHandler*>::iterator it = fdHandlers.find(fd);
    if (it != fdHandlers.end())
    {
        delete it->second;
        fdHandlers.erase(it);
    }
    ::close(fd);
}

const std::vector<ServerConfig>& Server::getConfigs() const
{
    return configs;
}

void Server::runEventLoop()
{
    const int MAX_EVENTS = 1024;
    epoll_event readyEvents[MAX_EVENTS];

    while (true)
    {
        int ready = epoll_wait(epollFD, readyEvents, MAX_EVENTS, -1);
        for (int i = 0; i < ready; ++i)
        {
            int fd = readyEvents[i].data.fd;
            uint32_t currEvent = readyEvents[i].events;

            if (fdHandlers.find(fd) == fdHandlers.end())
                continue;
            IEventHandler* handler = fdHandlers[fd];
            if (currEvent & (EPOLLERR | EPOLLHUP))
            {
                removeHandler(fd);
                continue;
            }
            if (currEvent & EPOLLIN)
                handler->handleRead();
            if (currEvent & EPOLLOUT)
                handler->handleWrite();
        }
    }
}


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
    epollFD = epoll_create(1024);
    if (epollFD < 0)
        throw ServerException("Server", "epoll_create() failed");
    std::map<int, std::vector<ServerConfig> > groupedConfigs;
    for (size_t i = 0; i < configs.size(); ++i) 
    {
        groupedConfigs[configs[i].port].push_back(configs[i]);
    }
    std::map<int, std::vector<ServerConfig> >::iterator it;
    for (it = groupedConfigs.begin(); it != groupedConfigs.end(); ++it)
    {
        Listener* listener = new Listener(it->second); 
        listeners[listener->getSocketFD()] = listener;
        registerToEpoll(listener->getSocketFD(), EPOLLIN);
    }
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
        clientMap.erase(clientFD);
        return;
    }
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




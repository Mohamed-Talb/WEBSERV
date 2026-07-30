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

const std::vector<ServerConfig>& Server::getConfigs() const
{
    return configs;
}

void Server::checkTimeout()
{
    time_t currentTime = std::time(NULL);
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
            handler->handleEvent(fd, currEvent);
        }
        checkTimeout();
    }
    // cleanup funtion needed here for when we break off the loop
}

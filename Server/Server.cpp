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

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
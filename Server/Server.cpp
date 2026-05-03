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

void Server::init(const std::vector<ServerConfig> &confs)
{
    configs = confs;
    epollFD = epoll_create(1024);
    if (epollFD < 0)
        throw ServerException("Server", "epoll_create failed");
    
    std::map<std::string, std::vector<ServerConfig> > groupedConfigs;
    for (size_t i = 0; i < configs.size(); ++i) 
    {
        std::string ipPort = configs[i].host + ":" + intToString(configs[i].port);
        groupedConfigs[ipPort].push_back(configs[i]);
    }
    std::map<std::string, std::vector<ServerConfig> >::iterator it;
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

const std::vector<ServerConfig>& Server::getConfigs() const
{
    return configs;
}

void Server::checkTimeout()
{
    time_t curr_time = time(NULL);
    for (std::map<int, IEventHandler *>::iterator i = fdHandlers.begin(); i != fdHandlers.end();)
    {
        Client *client = dynamic_cast<Client *>(i->second);
        if (client != NULL)
        {
            if (client->state != PROCESSING_CGI && difftime(curr_time, client->timeout) > 30) // hardcoded to 30 sec for now
            {
                removeHandler(i++->first);
                continue;
            }
        }
        i++;
    }
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
}

void Server::runEventLoop()
{
    const int MAX_EVENTS = 1024;
    epoll_event readyEvents[MAX_EVENTS];

    while (true)
    {
        int ready = epoll_wait(epollFD, readyEvents, MAX_EVENTS, 1000);
        if (ready == -1)
        {
            if (errno == EINTR) // os just wanted us to check a signal
                continue;
            break; // To-Do: unrecoverable error, clean up resources and inform the user using perror
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
                removeHandler(fd);
                continue;
            }
            if (currEvent & EPOLLIN)
            {
                handler->handleRead();
                if (fdHandlers.find(fd) == fdHandlers.end())
                    continue;
            }
            if (currEvent & EPOLLOUT)
                handler->handleWrite();
        }
        checkTimeout();
    }
    // cleanup funtion needed here for when we break off the loop
}
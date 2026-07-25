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
        addHandlerFD(listener, listener->getFD(), EPOLLIN);
    }
}

void Server::addHandlerFD(IEventHandler *handler, int fd, uint32_t events)
{
    if (handler == NULL)
        throw std::runtime_error("SERVER: null event handler");

    if (fd < 0)
        throw std::runtime_error("SERVER: invalid handler descriptor");

    if (fdHandlers.count(fd) != 0)
        throw std::runtime_error("SERVER: handler descriptor already registered");

    epoll_event event;
    std::memset(&event, 0, sizeof(event));

    event.events = events;
    event.data.fd = fd;

    if (epoll_ctl(epollFD,EPOLL_CTL_ADD,fd,&event) < 0)
    {
        throw std::runtime_error("SERVER: epoll_ctl ADD failed");
    }
    fdHandlers[fd] = handler;
}

void Server::modifyHandler(IEventHandler *handler, uint32_t events)
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
    std::vector<Client *> expiredClients;
    std::vector<Client *> cgiTimeoutClients;

    for (std::map<int, IEventHandler *>::iterator it = fdHandlers.begin(); it != fdHandlers.end(); it++)
    {
        Client *client = dynamic_cast<Client *>(it->second);
        if (client != NULL && difftime(curr_time, client->timeout) > TIMEOUT_DURATION)
        {
            if (client->getState() != PROCESSING_CGI)
                expiredClients.push_back(client);
            else
                cgiTimeoutClients.push_back(client);
        }
    }

    // client timeout
    for (size_t i = 0; i < expiredClients.size(); i++)
        removeHandler(expiredClients[i]->getFD());

    // cgi timeout
    for (size_t i = 0; i < cgiTimeoutClients.size(); i++)
    {
        Client *client = cgiTimeoutClients[i];
        client->terminateCgi();
    }
}

void Server::removeHandler(int fd)
{
    std::map<int, IEventHandler *>::iterator it = fdHandlers.find(fd);

    if (it == fdHandlers.end())
        return;

    IEventHandler *handler = it->second;
    epoll_ctl(epollFD, EPOLL_CTL_DEL, fd, NULL);

    fdHandlers.erase(it);
    deletionQueue.insert(handler);
}

void Server::clearDeletionQueue()
{
    while (!deletionQueue.empty())
    {
        IEventHandler* handler = *deletionQueue.begin();
        deletionQueue.erase(deletionQueue.begin());
        delete handler;
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

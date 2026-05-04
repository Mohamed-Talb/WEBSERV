#include "Server.hpp"
#include <sys/epoll.h>
#include <unistd.h>
#include <errno.h>
#include <stdexcept>
#include <set>

Server::Server() : epollFD(-1) {}

Server::~Server()
{
    /*
        Important:

        With the new design, multiple fds can point to the same handler.
        Example:
            fdHandlers[cgiStdoutFd] = cgiHandler;
            fdHandlers[cgiStdinFd]  = cgiHandler;

        So we must not blindly delete every map value, because that can
        double-delete the same object.

        This set makes sure each handler is deleted only once.
    */
    std::set<IEventHandler *> deletedHandlers;

    std::map<int, IEventHandler*>::iterator it;
    for (it = fdHandlers.begin(); it != fdHandlers.end(); ++it)
    {
        if (deletedHandlers.insert(it->second).second)
            delete it->second;

        ::close(it->first);
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
        Listener *listener = new Listener(it->second, this);

        /*
            New addHandler signature:
                addHandler(fd, handler, events)

            Listener still has getFD() as a convenience method,
            but getFD() is no longer part of IEventHandler.
        */
        addHandler(listener->getFD(), listener, EPOLLIN);
    }
}

void Server::addHandler(int fd, IEventHandler *handler, uint32_t events)
{
    fdHandlers[fd] = handler;

    epoll_event ev;
    ev.events = events;
    ev.data.fd = fd;

    if (epoll_ctl(epollFD, EPOLL_CTL_ADD, fd, &ev) == -1)
    {
        fdHandlers.erase(fd);
        throw ServerException("Server", "epoll_ctl ADD failed");
    }
}

void Server::modifyHandler(int fd, IEventHandler *handler, uint32_t events)
{
    fdHandlers[fd] = handler;

    epoll_event ev;
    ev.events = events;
    ev.data.fd = fd;

    if (epoll_ctl(epollFD, EPOLL_CTL_MOD, fd, &ev) == -1)
        throw ServerException("Server", "epoll_ctl MOD failed");
}

const std::vector<ServerConfig>& Server::getConfigs() const
{
    return configs;
}

void Server::checkTimeout()
{
    time_t curr_time = time(NULL);

    std::map<int, IEventHandler*>::iterator it = fdHandlers.begin();

    while (it != fdHandlers.end())
    {
        Client *client = dynamic_cast<Client*>(it->second);

        if (client != NULL)
        {
            if (client->state != PROCESSING_CGI &&
                difftime(curr_time, client->timeout) > 30)
            {
                int fd = it->first;
                ++it;
                removeHandler(fd);
                continue;
            }
        }
        ++it;
    }
}

void Server::removeHandler(int fd)
{
    epoll_ctl(epollFD, EPOLL_CTL_DEL, fd, NULL);

    std::map<int, IEventHandler*>::iterator it = fdHandlers.find(fd);
    if (it == fdHandlers.end())
        return;

    IEventHandler *handler = it->second;

    fdHandlers.erase(it);

    /*
        Important:

        This version still deletes the handler when one fd is removed.

        That is okay for Listener and Client because:
            one handler = one fd

        But for future CGI where:
            one CgiHandler = multiple fds

        this can be dangerous. If cgiHandler is registered with two fds,
        removing one fd and deleting the object would leave the other fd
        pointing to deleted memory.

        For now this is okay until you implement multi-fd CgiHandler.

        Later, for CGI, you should either:
            - add a removeFdOnly() function, or
            - let CgiHandler manage its own lifetime, or
            - add reference counting / owned-fd tracking.
    */
    delete handler;
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
            if (errno == EINTR)
                continue;

            break;
        }

        for (int i = 0; i < ready; ++i)
        {
            int fd = readyEvents[i].data.fd;
            uint32_t currEvent = readyEvents[i].events;

            std::map<int, IEventHandler*>::iterator it = fdHandlers.find(fd);
            if (it == fdHandlers.end())
                continue;

            IEventHandler *handler = it->second;

            if (currEvent & (EPOLLERR | EPOLLHUP))
            {
                removeHandler(fd);
                continue;
            }

            if (currEvent & EPOLLIN)
            {
                handler->handleRead(fd);

                if (fdHandlers.find(fd) == fdHandlers.end())
                    continue;
            }

            if (currEvent & EPOLLOUT)
            {
                handler->handleWrite(fd);

                if (fdHandlers.find(fd) == fdHandlers.end())
                    continue;
            }
        }

        checkTimeout();
    }
}
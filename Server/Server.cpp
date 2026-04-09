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

void Server::init(vector<ServerConfig>& configs)
{
    epollFD = epoll_create(1024);
    if (epollFD < 0)
        throw ServerException("Server", "epoll_create() failed");
    for (size_t i = 0; i < configs.size(); ++i)
    {
        Listener* listener = new Listener(configs[i]); // throws on socket/bind/listen fail
        listeners[listener->getSocketFD()] = listener;
        registerToEpoll(listener->getSocketFD(), EPOLLIN); // throws on epoll_ctl fail
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
}

// SERVER RUNING
void Server::handleNewConnection(Listener* listener)
{
    Client* client = listener->acceptClient();
    if (!client)
        return;
    int clientFD = client->getSocketFD();

    epoll_event ev;
    ev.events  = EPOLLIN;
    ev.data.fd = clientFD;
    if (epoll_ctl(epollFD, EPOLL_CTL_ADD, clientFD, &ev) < 0)
    {
        logError("handleNewConnection", "epoll_ctl ADD failed for client fd " + intToString(clientFD));
        listener->removeClient(clientFD);
        return;
    }
}

void Server::handleClientWrite(Listener* listener, Client* client)
{
    if (!client->hasPendingWrite())
        return;

    int clientFD = client->getSocketFD();
    const std::string& buffer = client->getWriteBuffer();

    int bytes = send(clientFD, buffer.c_str(), buffer.size(), 0);

    if (bytes < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return; // kernel buffer full — epoll will notify us when ready again
        logError("handleClientWrite", "send() failed for fd " + intToString(clientFD));
        disconnectClient(listener, clientFD);
        return;
    }
    client->consumeWriteBuffer(bytes);
    if (!client->hasPendingWrite())
    {
        epoll_event ev;
        ev.events  = EPOLLIN;
        ev.data.fd = clientFD;
        if (epoll_ctl(epollFD, EPOLL_CTL_MOD, clientFD, &ev) < 0)
        {
            logError("handleClientWrite", "epoll_ctl MOD failed for fd " + intToString(clientFD));
            disconnectClient(listener, clientFD);
        }
    }
}


void Server::handleClientRead(Listener* listener, Client* client)
{
    char buf[4096];
    int  clientFD = client->getSocketFD();
    int  bytes    = recv(clientFD, buf, sizeof(buf), 0);

    if (bytes < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return; // no data yet — not an error
        logError("handleClientRead", "recv() failed for fd " + intToString(clientFD));
        disconnectClient(listener, clientFD);
        return;
    }

    if (bytes == 0)
    {
        disconnectClient(listener, clientFD);
        return;
    }

    client->appendToReadBuffer(std::string(buf, bytes));
    if (client->getReadBuffer().find("\r\n\r\n") != std::string::npos)
    {
        // full request received — build response
        // (this will be the HTTP parser entry point later)
        std::string response =
            "HTTP/1.1 200 OK\r\n"
            "Content-Length: 13\r\n"
            "Content-Type: text/plain\r\n"
            "Connection: close\r\n"
            "\r\n"
            "Hello, World!";
        client->appendToWriteBuffer(response);
        client->clearReadBuffer();
        epoll_event ev;
        ev.events  = EPOLLIN | EPOLLOUT;
        ev.data.fd = clientFD;
        if (epoll_ctl(epollFD, EPOLL_CTL_MOD, clientFD, &ev) < 0)
        {
            logError("handleClientRead", "epoll_ctl MOD failed for fd " + intToString(clientFD));
            disconnectClient(listener, clientFD);
        }
    }
}

void Server::runEventLoop()
{
    const int   MAX_EVENTS = 1024;
    epoll_event readyEvents[MAX_EVENTS];
    while (true)
    {
        int ready = epoll_wait(epollFD, readyEvents, MAX_EVENTS, -1);
        if (ready < 0)
        {
            if (errno == EINTR)
                continue;
            throw ServerException("runEventLoop", "epoll_wait() failed");
        }
        for (int i = 0; i < ready; ++i)
        {
            int fd = readyEvents[i].data.fd;
            std::map<int, Listener*>::iterator it = listeners.find(fd);
            if (it != listeners.end())
            {
                handleNewConnection(it->second);
                continue;
            }
            Client*   client         = NULL;
            Listener* parentListener = NULL;

            for (it = listeners.begin(); it != listeners.end(); ++it)
            {
                client = it->second->getClient(fd);
                if (client)
                {
                    parentListener = it->second;
                    break;
                }
            }
            if (!client)
                continue;
            uint32_t events = readyEvents[i].events;
            if (events & (EPOLLHUP | EPOLLERR))
            {
                disconnectClient(parentListener, fd);
                continue;
            }
            if (events & EPOLLIN)
            {
                handleClientRead(parentListener, client);
                continue;
            }
            if (events & EPOLLOUT)
                handleClientWrite(parentListener, client);
        }
    }
}




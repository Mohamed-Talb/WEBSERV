#include "Server.hpp"
Server::Server() : epollFD(-1) {}

// Server::Server(const Server& other)
// {
//     listeners = other.listeners;
// }

// Server& Server::operator=(const Server& other)
// {
//     if (this != &other)
//         listeners = other.listeners;
//     return *this;
// }

Server::~Server()
{
    shutdown();
}

void Server::loadListeners(vector<ServerConfig> &configs)
{
    for (size_t i = 0; i < configs.size(); ++i)
        addListener(new Listener(configs[i]));
}

void Server::addListener(Listener* listener)
{
    listeners[listener->getSocketFD()] = listener;
}

void Server::initEventSystem()
{
    epollFD = epoll_create(1024);
    if (epollFD < 0)
    {
        perror("epoll_create");
        exit(1);
    }

    for (map<int, Listener*>::iterator it = listeners.begin(); it != listeners.end(); ++it)
    {
        int listenerFD = it->first;
        // Listener* listener = it->second;   ??????
        epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.fd = listenerFD;
        if (epoll_ctl(epollFD, EPOLL_CTL_ADD, listenerFD, &ev) < 0)
        {
            perror("epoll_ctl: add listener");
            exit(1);
        }
    }
}

void Server::handleNewConnection(Listener* listener)
{
    Client* client = listener->acceptClient();
    if (!client)
        return;

    int clientFD = client->getSocketFD();
    epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = clientFD;
    if (epoll_ctl(epollFD, EPOLL_CTL_ADD, clientFD, &ev) < 0)
    {
        perror("epoll_ctl add client");
        delete client;
    }
}


void Server::handleClientWrite(Listener* listener, Client* client)
{
    (void)listener;
    string& buffer = client->getWriteBuffer();
    if (buffer.empty())
        return;
    int bytes = send(client->getSocketFD(), buffer.c_str(), buffer.size(), 0);
    if (bytes < 0) 
    {
        if (errno != EAGAIN && errno != EWOULDBLOCK) 
        {
            perror("send");
            client->closeConnection();
        }
        return;
    }
    buffer.erase(0, bytes);
    if (buffer.empty())
    {
        epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.fd = client->getSocketFD();
        epoll_ctl(epollFD, EPOLL_CTL_MOD, client->getSocketFD(), &ev);
    }
}

void Server::handleClientRead(Listener* listener, Client* client)
{
    (void)listener;
    char buf[4096];
    int bytes = recv(client->getSocketFD(), buf, sizeof(buf), 0);
    if (bytes < 0) 
    {
        if (errno != EAGAIN && errno != EWOULDBLOCK) 
        {
            perror("recv");
            client->closeConnection();
        }
        return;
    }
    if (bytes == 0) 
    {
        client->closeConnection();
        return;
    }
    client->appendToReadBuffer(std::string(buf, bytes));
    if (client->getReadBuffer().find("\r\n\r\n") != string::npos)
    {
        string response =
            "HTTP/1.1 200 OK\r\n"
            "Content-Length: 13\r\n"
            "Content-Type: text/plain\r\n"
            "\r\n"
            "Hello, World!";

        client->appendToWriteBuffer(response);

        epoll_event ev;
        ev.events = EPOLLIN | EPOLLOUT;
        ev.data.fd = client->getSocketFD();
        epoll_ctl(epollFD, EPOLL_CTL_MOD, client->getSocketFD(), &ev);
    }
}

void Server::runEventLoop()
{
    const int MAX_EVENTS = 1024;
    epoll_event readyEvents[MAX_EVENTS];
    while (true)
    {
        int ready = epoll_wait(epollFD, readyEvents, MAX_EVENTS, -1);
        if (ready < 0)
        {
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < ready; ++i)
        {
            int fd = readyEvents[i].data.fd;

            Listener* listener = NULL;
            map<int, Listener*>::iterator it = listeners.find(fd);
            if (it != listeners.end())
            {
                listener = it->second;
                handleNewConnection(listener);
                continue;
            }

            Client* client = NULL;
            Listener* parentListener = NULL;

            for (it = listeners.begin(); it != listeners.end(); ++it)
            {
                client = it->second->getClients()[fd];
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
                epoll_ctl(epollFD, EPOLL_CTL_DEL, fd, NULL);
                if (parentListener)
                {
                    parentListener->removeClient(fd);
                }
                continue;
            }

            if (events & EPOLLIN)
            {
                handleClientRead(parentListener, client);
                cout << client->getReadBuffer() << endl;
            }
            if (events & EPOLLOUT)
                handleClientWrite(parentListener, client);
        }
    }
}

void Server::shutdown()
{
    for (map<int, Listener*>::iterator it = listeners.begin(); it != listeners.end(); ++it)
    {
        Listener* listener = it->second;
        listener->closeListener();
        delete listener;
    }
    listeners.clear();

    if (epollFD >= 0)
    {
        ::close(epollFD);
        epollFD = -1;
    }
}




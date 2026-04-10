#include "../Server/Server.hpp"

Client::Client(int fd) : socketFD(fd) {}

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

void Client::appendToReadBuffer(const std::string& data)
{
    readBuffer += data;
}

const std::string& Client::getReadBuffer() const
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

const std::string& Client::getWriteBuffer() const
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

int Client::getSocketFD() const
{
    return socketFD;
}


// ─── Listener.cpp ───────────────────────────────────────────────────
#include "Server.hpp"

Listener::Listener(const ServerConfig& conf): socketFD(-1), config(conf)
{
    loadListener(conf);
}

Listener::~Listener()
{
    closeListener();
}

void Listener::loadListener(const ServerConfig& conf)
{
    socketFD = socket(AF_INET, SOCK_STREAM, 0);
    if (socketFD < 0)
        throw ServerException("Listener","socket() failed on port " + intToString(conf.port));
    int opt = 1;
    if (setsockopt(socketFD, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        ::close(socketFD);
        socketFD = -1;
        throw ServerException("Listener",
            "setsockopt() failed on port " + intToString(conf.port));
    }

    sockaddr_in addr;
    // std::memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(conf.port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(socketFD, (sockaddr*)&addr, sizeof(addr)) < 0)
    {
        ::close(socketFD);
        socketFD = -1;
        throw ServerException("Listener","bind() failed on port " + intToString(conf.port));
    }

    if (listen(socketFD, SOMAXCONN) < 0)
    {
        ::close(socketFD);
        socketFD = -1;
        throw ServerException("Listener","listen() failed on port " + intToString(conf.port));
    }
}

void Listener::closeListener()
{
    for (std::map<int, Client*>::iterator it = clients.begin(); it != clients.end(); ++it)
    {
        it->second->closeConnection();
        delete it->second;
    }
    clients.clear();
    if (socketFD >= 0)
    {
        ::close(socketFD);
        socketFD = -1;
    }
}


Client* Listener::acceptClient()
{
    int clientFD = accept(socketFD, NULL, NULL);
    if (clientFD < 0)
        return NULL; // EAGAIN/EWOULDBLOCK — no client ready, not an error
    if (fcntl(clientFD, F_SETFL, O_NONBLOCK) < 0)
    {
        ::close(clientFD);
        throw ServerException("Listener","fcntl() failed for new client on port " + intToString(config.port));
    }
    Client* client       = new Client(clientFD);
    clients[clientFD]    = client;
    return client;
}

Client* Listener::getClient(int fd) const
{
    std::map<int, Client*>::const_iterator it = clients.find(fd);
    if (it != clients.end())
        return it->second;
    return NULL;
}

void Listener::removeClient(int clientFD)
{
    std::map<int, Client*>::iterator it = clients.find(clientFD);
    if (it == clients.end())
        return;
    it->second->closeConnection();
    delete it->second;
    clients.erase(it);
}

int Listener::getSocketFD() const { return socketFD; }
int Listener::getPort()     const { return config.port; }



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
    cout << client->getReadBuffer(); 
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



#include "Server/Server.hpp"

std::vector<ServerConfig> GETconfig();
int main()
{
    try
    {
        std::vector<ServerConfig> configs = GETconfig();
        Server server;
        server.init(configs);
        server.runEventLoop();
    }
    catch (const ServerException& e)
    {
        std::cerr << e.what() << "\n";
        return 1;
    }
}
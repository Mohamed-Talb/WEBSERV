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
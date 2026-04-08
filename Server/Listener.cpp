#include "Server.hpp"

Listener::Listener() : socketFD(-1) {}

Listener::Listener(const ServerConfig& conf)
{
    loadListener(conf);
}

Listener::~Listener()
{
    closeListener();
}

void Listener::loadListener(const ServerConfig &conf)
{
    config = conf; /// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    socketFD = socket(AF_INET, SOCK_STREAM, 0);
    if (socketFD < 0)
    {
        std::cerr << "Socket creation failed\n";
        return;
    }
    int opt = 1;
    if (setsockopt(socketFD, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        perror("setsockopt SO_REUSEADDR");
        exit(1);
    }
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(config.port);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(socketFD, (sockaddr*)&addr, sizeof(addr)) < 0)
    {
        std::cerr << "Bind failed\n";
        return;
    }
    if (listen(socketFD, SOMAXCONN) < 0)
    {
        std::cerr << "Listen failed\n";
        return;
    }
}

Client* Listener::acceptClient()
{
    int clientFD = accept(socketFD, NULL, NULL);
    if (clientFD < 0)
    {
        perror("accept");
        return NULL;
    }
    fcntl(clientFD, F_SETFL, O_NONBLOCK);

    Client* client = new Client(clientFD);
    clients[clientFD] = client;
    return client;
}

Client* Listener::getClient(int fd)
{
    std::map<int, Client*>::iterator it = clients.find(fd);
    if (it != clients.end())
        return it->second;
    return NULL;
}

void Listener::removeClient(int clientFD)
{
    std::map<int, Client*>::iterator it = clients.find(clientFD);
    if (it != clients.end())
    {
        Client* client = it->second;
        client->closeConnection();
        delete client;
        clients.erase(it);
    }
}

void Listener::closeListener()
{
    for (std::map<int, Client*>::iterator it = clients.begin(); it != clients.end(); ++it)
    {
        Client* client = it->second;
        client->closeConnection();
        delete client;
    }
    clients.clear();
    if (socketFD >= 0)
    {
        ::close(socketFD);
        socketFD = -1;
    }
}

int Listener::getSocketFD() const
{
    return socketFD;
}

int Listener::getPort() const
{
    return config.port;
}

std::map<int, Client*>& Listener::getClients()
{
    return clients;
}
#include "Listener.hpp"



Listener::Listener(const std::vector<ServerConfig *> &confs, Server *srv)
    : socketFD(-1), server(srv), configs(confs)
{
    if (!server)
        throw std::runtime_error("LISTENER: invalid server");

    if (configs.empty() || !configs[0])
        throw std::runtime_error("LISTENER: no server configuration");

    ServerConfig *conf = configs[0];
    socketFD = socket(AF_INET, SOCK_STREAM, 0);

    if (socketFD < 0)
        throw std::runtime_error("LISTENER: socket() failed on port " + intToString(conf->port));

    int opt = 1;

    if (setsockopt(socketFD, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        ::close(socketFD);
        socketFD = -1;
        throw std::runtime_error("LISTENER: setsockopt() failed on port " + intToString(conf->port));
    }

    int flags = fcntl(socketFD, F_GETFL, 0);

    if (flags < 0 || fcntl(socketFD, F_SETFL, flags | O_NONBLOCK) < 0)
    {
        ::close(socketFD);
        socketFD = -1;
        throw std::runtime_error("LISTENER: fcntl() failed on port " + intToString(conf->port));
    }

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));

    addr.sin_family = AF_INET;
    addr.sin_port = htons(conf->port);
    addr.sin_addr.s_addr = inet_addr(conf->host.c_str());

    if (addr.sin_addr.s_addr == INADDR_NONE)
        addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(socketFD, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0)
    {
        ::close(socketFD);
        socketFD = -1;
        throw std::runtime_error("LISTENER: bind() failed on port " + intToString(conf->port));
    }
    if (listen(socketFD, SOMAXCONN) < 0)
    {
        ::close(socketFD);
        socketFD = -1;
        throw std::runtime_error("LISTENER: listen() failed on port " + intToString(conf->port));
    }
}

Listener::~Listener()
{
    if (socketFD >= 0)
    {
        ::close(socketFD);
        socketFD = -1;
    }
}

void Listener::handleEvent(int fd, uint32_t events)
{
    if (fd != socketFD)
        return;
    if (events & (EPOLLERR | EPOLLHUP))
    {
        server->removeHandler(this);
        return;
    }
    if (!(events & EPOLLIN))
        return;
    while (true)
    {
        int clientFD = accept(socketFD, NULL, NULL);
        if (clientFD >= 0)
        {
            int flags = fcntl(clientFD, F_GETFL, 0);
            if (flags < 0 || fcntl(clientFD, F_SETFL, flags | O_NONBLOCK) < 0)
            {
                ::close(clientFD);
                continue;
            }
            Client *newClient = NULL;
            try
            {
                newClient = new Client(clientFD, server, configs);
                server->addHandler(newClient, clientFD, EPOLLIN);
            }
            catch (...)
            {
                delete newClient;
                throw;
            }
            continue;
        }
        if (errno == EINTR)
            continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return;
        return;
    }
}

int Listener::getFD() const
{
    return socketFD;
}

int Listener::getPort() const
{
    if (configs.empty() || !configs[0])
        return -1;

    return configs[0]->port;
}
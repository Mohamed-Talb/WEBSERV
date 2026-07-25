#include "Listener.hpp" 

Listener::Listener(const std::vector<ServerConfig *> confs, Server *srv) : socketFD(-1), server(srv), configs(confs)
{
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
    if (fcntl(socketFD, F_SETFL, O_NONBLOCK) < 0)
    {
        ::close(socketFD);
        socketFD = -1;
        throw std::runtime_error("LISTENER: fcntl() failed on port " + intToString(conf->port));
    }
    sockaddr_in addr;
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(conf->port);
    addr.sin_addr.s_addr = inet_addr(conf->host.c_str()); 

    if (addr.sin_addr.s_addr == INADDR_NONE)
        addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(socketFD, (sockaddr*)&addr, sizeof(addr)) < 0)
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
    if (fd == socketFD && (events & EPOLLIN))
    {
        while (true)
        {
            int clientFD = accept(socketFD, NULL, NULL);
            if (clientFD < 0)
            {
                break; 
            }
            if (fcntl(clientFD, F_SETFL, O_NONBLOCK) < 0)
            {
                ::close(clientFD);
                continue;
            }
            Client* newClient = new Client(clientFD, server, configs);
            server->addHandlerFD(newClient, clientFD, EPOLLIN); 
        }
    }
}

int Listener::getFD() const {return socketFD;}
int Listener::getPort() const {return configs[0]->port;}

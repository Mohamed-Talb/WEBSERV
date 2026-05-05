#ifndef LISTENER_HPP
#define LISTENER_HPP

#include "Server.hpp"

class Listener : public IEventHandler 
{
    private:
    int socketFD;
    Server* server;
    std::vector<ServerConfig> configs;

    Listener();
    Listener(const Listener&);

    public:
    virtual ~Listener();
    Listener(const std::vector<ServerConfig> &confs, Server *srv);

    virtual void handleRead(int fd);
    virtual void handleWrite(int fd);

    int getFD() const;
    int getPort() const;
};

#endif
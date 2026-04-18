#ifndef LISTENER_HPP
#define LISTENER_HPP
#include "../Lib.hpp"
#include "../configParser/configParser.hpp"
#include "Client.hpp"
#include "../Errors.hpp"
#include "../HTTP/HttpRequest.hpp"
#include "../HTTP/HttpResponse.hpp"
#include "../HTTP/HttpHandler.hpp"

enum IOState 
{
    IO_READY,        // Replaces 0: The operation finished, switch epoll state
    IO_PENDING,      // Replaces 1: The operation needs more time, wait for epoll
    IO_DISCONNECTED  // Replaces 2: The client dropped, kill the connection
};

class Listener
{
    private:
        int                      socketFD;
        std::vector<ServerConfig> configs;
        std::map<int, Client*>   clients;
        void loadListener(const ServerConfig &conf);
        void closeListener(); 
        Listener();
        Listener(const Listener&);
        Listener& operator=(const Listener&);

    public:
        Listener(const std::vector<ServerConfig>& confs);
        ~Listener();
        // client lifecycle — called by Server
        Client* acceptClient();
        Client* getClient(int fd) const;
        void    removeClient(int clientFD);
        const ServerConfig& matchConfig(const std::string& hostHeader) const;
        IOState handleClientRead(Client* client);
        IOState handleClientWrite(Client* client);
        int         getSocketFD() const;
        int         getPort()     const;
};

#endif
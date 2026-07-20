#ifndef SERVER_HPP
#define SERVER_HPP

#include <map>
#include <ctime>
#include <vector>
#include <string>
#include <stdint.h>
#include <csignal>
#include <iostream>

#include "Client.hpp"
#include "Listener.hpp"
#include "../Helpers.hpp"
#include "../HTTP/HttpHandler.hpp"
#include "../configParser/configParser.hpp"
#include "Server.ipp"

#define TIMEOUT_DURATION 30

class Server
{
    private:
        int epollFD;
        std::vector<ServerConfig>     configs;
        std::map<int, IEventHandler*> fdHandlers;
        std::vector<IEventHandler*>   deletionQueue;

    public:
        Server();
        ~Server();

        void checkTimeout();
        void eventLoop();
        void init(const std::vector<ServerConfig> &confs);

        void removeHandler(int fd);
        void clearDeletionQueue();
        void addHandler(IEventHandler* handler, uint32_t events);
        void modifyHandler(IEventHandler* handler, uint32_t events);

        const std::vector<ServerConfig> &getConfigs() const;
};


#endif
#ifndef SERVER_HPP
#define SERVER_HPP

#include <set>
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
#include "Session.hpp"
#include "Server.ipp"
#include <map>
#include <set>
#include <vector>

#define TIMEOUT_DURATION 30
#define SESSION_TIMEOUT 1800
#define SESSION_CLEANUP_INTERVAL 60

class Server
{
    private:
    int epollFD;
    time_t lastSessionCleanup;
    SessionManager sessionManager;
    std::vector<ServerConfig>     configs;
    std::map<int, IEventHandler*> fdHandlers;
    std::set<IEventHandler*>   deletionQueue;
    std::map<IEventHandler *, std::set<int> > registeredFds;

    public:
    Server();
    ~Server();

    void eventLoop();
    void checkTimeout();
    void init(const std::vector<ServerConfig> &confs);

    void clearDeletionQueue();
    void unregisterFD(int fd);
    void removeHandler(IEventHandler *handler);
    void modifyHandler(int fd, uint32_t events);
    void addHandler(IEventHandler *handler, int fd, uint32_t events);
    
    
    SessionManager &getSessionManager();
    const std::vector<ServerConfig> &getConfigs() const;
};


#endif
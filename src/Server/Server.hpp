#ifndef SERVER_HPP
#define SERVER_HPP

#include <map>
#include <set>
#include <ctime>
#include <string>
#include <vector>
#include <cerrno>
#include <cstring>
#include <stdint.h>
#include <unistd.h>
#include <stdexcept>
#include <csignal>
#include "Server.ipp"
#include <sys/epoll.h>
#include "../Helpers.hpp"
#include "../configParser/configParser.hpp"

class Client;
class Listener;

#define TIMEOUT_DURATION 30
#define SESSION_TIMEOUT 1800
#define SESSION_CLEANUP_INTERVAL 60

typedef std::map<int, IEventHandler *> FDHandlerMap;
typedef std::map<IEventHandler *, std::set<int> > HandlerFDMap;
typedef std::map<std::string, std::vector<ServerConfig *> > ServerConfigMap;

class Server
{
    private:
        int epollFD;
        std::vector<ServerConfig>     configs;
        std::map<int, IEventHandler*> fdHandlers;
        std::set<IEventHandler*>   deletionQueue;
        std::map<IEventHandler *, std::set<int> > handlers;
    public:
        Server();
        ~Server();

        void eventLoop();
        void checkTimeout();
        
        void clearDeletionQueue();
        void unregisterFD(int fd);
        void removeHandler(IEventHandler *handler);
        void modifyHandler(int fd, uint32_t events);
        void init(const std::vector<ServerConfig> &confs);
        const std::vector<ServerConfig> &getConfigs() const;
        void addHandler(IEventHandler *handler, int fd, uint32_t events);
};


#endif
#ifndef SERVER_HPP
#define SERVER_HPP

#include "../configParser/configParser.hpp"
#include "../Lib.hpp"
#include "Listener.hpp"

using namespace std;

class Server
{
    private:
        map<int, Listener*> listeners;
        std::map<int, Listener*> clientMap;
        int epollFD;
        void registerToEpoll(int fd, uint32_t events);
        void disconnectClient(Listener* listener, int clientFD);
        void handleConnection(Listener *listener, int clientFd, uint32_t event);
        void handleNewConnection(Listener* listener);

    public:
        Server();
        ~Server();
        void init(vector<ServerConfig>& configs);           // setup
        void runEventLoop();                               // run
        void shutdown(); 
};

#endif








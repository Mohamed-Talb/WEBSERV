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
        int epollFD;

    public:
        Server();
        // Server(const Server& other) = delete;
        // Server& operator=(const Server& other) = delete;
        ~Server();

        void loadListeners(vector<ServerConfig> &ports);
        void addListener(Listener* listener);

        void initEventSystem();
        // void registerListener(Listener* listener); // IMPLEMENTATION ..... ??? 
 
        void handleNewConnection(Listener* listener);
        void handleClientRead(Listener* listener,Client* client);
        void handleClientWrite(Listener* listener, Client* client);

        void runEventLoop();
        void shutdown();
};

#endif
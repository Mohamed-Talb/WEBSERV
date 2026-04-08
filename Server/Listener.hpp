#ifndef LISTENER_HPP
#define LISTENER_HPP
#include "../Lib.hpp"
#include "../configParser/configParser.hpp"
#include "Client.hpp"

class Listener
{
    private:
        int socketFD;
        ServerConfig config;
        std::map<int, Client*> clients;

    public:
        Listener();
        Listener(const ServerConfig& conf);
        ~Listener();

        void loadListener(const ServerConfig& conf);   // Create socket, bind, listen
        Client* acceptClient();                        // Accept new client
        Client* getClient(int fd);                     // Get client by FD
        void removeClient(int clientFD);               // Remove client
        void closeListener();                          // Close all clients + listening socket

        int getSocketFD() const;
        int getPort() const;

        std::map<int, Client*>& getClients();
};

#endif
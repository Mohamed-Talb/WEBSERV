#ifndef LISTENER_HPP
#define LISTENER_HPP
#include "../Lib.hpp"
#include "../configParser/configParser.hpp"
#include "Client.hpp"
#include "../Errors.hpp"
// class Listener
// {
//     private:
//         int socketFD;
//         ServerConfig config;
//         std::map<int, Client*> clients;

//     public:
//         Listener();
//         Listener(const ServerConfig &conf);
//         ~Listener();

//         void loadListener(const ServerConfig& conf);   // Create socket, bind, listen
//         Client* acceptClient();                        // Accept new client
//         Client* getClient(int fd);                     // Get client by FD
//         void removeClient(int clientFD);               // Remove client
//         void closeListener();                          // Close all clients + listening socket

//         int getSocketFD() const;
//         int getPort() const;

//         std::map<int, Client*>& getClients();
// };

// ─── Listener.hpp ───────────────────────────────────────────────────

enum IOState 
{
    IO_READY,        // Replaces 1: The operation finished, switch epoll state
    IO_PENDING,      // Replaces 0: The operation needs more time, wait for epoll
    IO_DISCONNECTED  // Replaces -1: The client dropped, kill the connection
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
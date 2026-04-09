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


class Listener
{
    private:
        int                      socketFD;
        ServerConfig             config;
        std::map<int, Client*>   clients;
        void loadListener(const ServerConfig& conf);
        void closeListener(); 
        Listener();
        Listener(const Listener&);
        Listener& operator=(const Listener&);

    public:
        explicit Listener(const ServerConfig& conf);
        ~Listener();
        // client lifecycle — called by Server
        Client* acceptClient();
        Client* getClient(int fd) const;
        void    removeClient(int clientFD);

        int         getSocketFD() const;
        int         getPort()     const;
};

#endif
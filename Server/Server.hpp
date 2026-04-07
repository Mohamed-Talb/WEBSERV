#ifndef SERVER_HPP
#define SERVER_HPP
#include <map>
#include <vector>
#include <iostream>

using namespace std;

class Client
{
private:
    int         socketFd;
    int         serverFd;
    std::string bufferRead;
    std::string bufferWrite;

public:
    /* Canonical Form */
    Client();
    Client(int clientFd, int serverFd);
    Client(const Client& other);
    Client& operator=(const Client& other);
    ~Client();

    void readData(); 
    void writeData(); 

    void processRequest();

    void closeConnection(); 

    int  getSocketFd() const;
    int  getServerFd() const;
    bool hasPendingWrite() const;

    void appendToReadBuffer(const std::string& data);
    void appendToWriteBuffer(const std::string& data);
};

class Server
{
private:
    int                 socketFd;
    int                 port;
    std::vector<Client*> clients;

public:
    /* Canonical Form */
    Server();
    Server(int port);
    Server(const Server& other);
    Server& operator=(const Server& other);
    ~Server();

    void bindListen();

    Client* acceptClient(); 
    void    addClient(Client* client);
    void    removeClient(int clientFd);

    int getSocketFd() const;
    int getPort() const;

    std::vector<Client*>& getClients();

    void closeServer();
};

class ServerManager
{
private:
    std::vector<Server*> servers;
    int                  epollFd;   // or poll/select structure

public:
    /* Canonical Form */
    ServerManager();
    ServerManager(const ServerManager& other);
    ServerManager& operator=(const ServerManager& other);
    ~ServerManager();

    void createServer(int port);   // from config
    void addServer(Server* server);

    void initEventSystem();        // epoll_create
    void registerServer(Server* server);

    void handleNewConnection(Server* server);
    void handleClientRead(Client* client);
    void handleClientWrite(Client* client);

    void runEventLoop();

    void shutdown();
};

#endif
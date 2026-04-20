#ifndef SERVER_HPP
#define SERVER_HPP

#include <map>
#include <vector>
#include "../Lib.hpp"
#include "Server.ipp"
#include "../Helpers.hpp"
#include "../HTTP/HttpRequest.hpp"
#include "../configParser/configParser.hpp"
#include "../Errors.hpp"

class Listener;

class Server
{
    private:
    int epollFD;
    std::vector<ServerConfig> configs;
    std::map<int, IEventHandler*> fdHandlers;

    public:
    Server();
    ~Server();

    void runEventLoop();
    void removeHandler(int fd);
    void init(const std::vector<ServerConfig>& confs);
    void addHandler(IEventHandler* handler, uint32_t events);
    void modifyHandler(IEventHandler* handler, uint32_t events);

    const std::vector<ServerConfig>& getConfigs() const;
};


class Client : public IEventHandler
{
    private:
    HttpRequest request;
    int         socketFD;
    std::string readBuffer;
    std::string writeBuffer;
    
    Server* server;
    std::vector<ServerConfig> configs;

    Client();
    Client(const Client&);
    const ServerConfig* matchConfig(const std::string& host) const;

    public:
    virtual ~Client();
    Client(int fd, Server* srv, const std::vector<ServerConfig>& confs);

    void closeConnection();
    bool isConnected() const;

    void clearReadBuffer();
    bool hasPendingWrite() const;
    void consumeReadBuffer(size_t bytes);
    void consumeWriteBuffer(size_t bytes);
    void appendToWriteBuffer(const std::string& data);
    void appendToReadBuffer(const char* data, size_t size);
    
    virtual void handleRead();
    virtual void handleWrite();

    HttpRequest &getRequest();
    const std::string &getReadBuffer() const;
    const std::string &getWriteBuffer() const;

    virtual int  getFD() const;
};


class Listener : public IEventHandler 
{
        private:
        int                       socketFD;
        std::vector<ServerConfig> configs;
        Server* server; 

        void closeListener(); 
        void loadListener(const ServerConfig &conf);
        
        Listener();
        Listener(const Listener&);
        Listener& operator=(const Listener&);

        public:
        virtual ~Listener();
        Listener(const std::vector<ServerConfig>& confs, Server* srv);
        
        virtual void handleRead();
        virtual void handleWrite();

        int getPort() const;
        virtual int  getFD() const;
};

#endif
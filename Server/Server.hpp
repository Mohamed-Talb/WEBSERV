#ifndef SERVER_HPP
#define SERVER_HPP

#include <map>
#include <vector>
#include "../Lib.hpp"
#include "Server.ipp"
#include "../Helpers.hpp"
#include "../HTTP/HttpHandler.hpp"
#include "../configParser/configParser.hpp"
#include "../Errors.hpp"


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


class Listener : public IEventHandler 
{
        private:
        int socketFD;
        Server* server; 
        std::vector<ServerConfig> configs;
        
        Listener();
        Listener(const Listener&);

        public:
        virtual ~Listener();
        Listener(const std::vector<ServerConfig>& confs, Server* srv);
        
        virtual void handleRead();
        virtual void handleWrite();

        int getPort() const;
        virtual int  getFD() const;
};

class Client : public IEventHandler
{
    private:
    int socketFD;
    Server* server;
    HttpRequest request;
    std::string readBuffer;
    std::string writeBuffer;
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
    virtual int getFD() const;
    const std::string &getReadBuffer() const;
    const std::string &getWriteBuffer() const;

};

#endif
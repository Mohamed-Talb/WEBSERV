#ifndef SERVER_HPP
#define SERVER_HPP

#include <map>
#include <vector>
#include <string>
#include <ctime>
#include <stdint.h>

#include "../Lib.hpp"
#include "Server.ipp"
#include "../Helpers.hpp"
#include "../HTTP/HttpHandler.hpp"
#include "../configParser/configParser.hpp"
#include "../Errors.hpp"

enum ClientState 
{
    READING_REQUEST,
    PROCESSING_CGI,
    SENDING_RESPONSE
};

class Server
{
private:
    int epollFD;
    std::vector<ServerConfig> configs;
    std::map<int, IEventHandler*> fdHandlers;

public:
    Server();
    ~Server();

    void checkTimeout();
    void runEventLoop();
    void removeHandler(int fd);
    void init(const std::vector<ServerConfig>& confs);

    void addHandler(int fd, IEventHandler* handler, uint32_t events);
    void modifyHandler(int fd, IEventHandler* handler, uint32_t events);

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

    virtual void handleRead(int fd);
    virtual void handleWrite(int fd);

    int getPort() const;
    int getFD() const;
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

    const ServerConfig* matchConfig(const std::string& host) const;

public:
    time_t timeout;
    ClientState state;

    virtual ~Client();
    Client(int fd, Server* srv, const std::vector<ServerConfig>& confs);

    bool isConnected() const;
    void onCgiDone(const HttpResponse& response);

    bool hasPendingWrite() const;
    void consumeReadBuffer(size_t bytes);
    void consumeWriteBuffer(size_t bytes);
    void appendToWriteBuffer(const std::string &data);
    void appendToReadBuffer(const char* data, size_t size);

    virtual void handleRead(int fd);
    virtual void handleWrite(int fd);

    HttpRequest& getRequest();
    int getFD() const;
    const std::string& getReadBuffer() const;
    const std::string& getWriteBuffer() const;
};

#endif
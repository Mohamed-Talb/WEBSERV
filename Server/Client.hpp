#ifndef CLIENT_HPP
#define CLIENT_HPP

#include "../Errors.hpp"
#include "../Helpers.hpp"
#include "../HTTP/HttpHandler.hpp"
#include "../configParser/configParser.hpp"

class Server;

enum ClientState 
{
    READING_REQUEST,
    PROCESSING_CGI,
    SENDING_RESPONSE
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
    CGI* activeCgi;
    
    const ServerConfig* matchConfig(const std::string& host) const;
    
    public:
    time_t timeout;
    ClientState state;

    virtual ~Client();
    Client(int fd, Server* srv, const std::vector<ServerConfig>& confs);

    bool isConnected() const;
    void onCgiDone(HttpResponse response);

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
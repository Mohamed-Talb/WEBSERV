#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <ctime>
#include <string>
#include <vector>

#include <stdint.h>
#include "Server.ipp"
#include "../HTTP/HttpRequestParser.hpp"
#include "../HTTP/HttpResponse.hpp"
#include "../configParser/configParser.hpp"

class Server;
class CGI;

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
    Server *server;

    const std::vector<ServerConfig *> configs;
    const ServerConfig *activeConfig;

    std::string readBuffer;
    std::string writeBuffer;

    HttpRequestParser requestParser;

    CGI *activeCgi;
    ClientState state;
    bool closeAfterWrite;
    size_t writeOffset;

    private:
    bool readFromSocket();
    void processReadBuffer();
    void closeConnection();
    void errorsHandler(int errorCode);
    void consumeReadBuffer(size_t bytes);
    // void consumeWriteBuffer(size_t bytes);
    
    void appendToWriteBuffer(const std::string &data);
    void appendToReadBuffer(const char *data, size_t size);
    const ServerConfig *matchConfig(const std::string &host) const;

    public:
    time_t timeout;

    Client(int fd, Server *server, const std::vector<ServerConfig*> configs);
    ~Client();

    void handleRead();
    void handleWrite();
    int getFD() const;
    void handleEvent(int, uint32_t);

    bool isConnected() const;
    bool hasPendingWrite() const;

    HttpRequest &getActiveRequest();

    const std::string &getReadBuffer() const;
    const std::string &getWriteBuffer() const;
    ClientState getState() const;

    void onCgiDone(HttpResponse response);
    void terminateCgi();
};

#endif
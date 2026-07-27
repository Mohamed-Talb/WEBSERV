#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <ctime>
#include <string>
#include <vector>

#include <stdint.h>
#include "Server.ipp"
#include "../HTTP/HttpResponse.hpp"
#include "../HTTP/HttpHandler.hpp"
#include "../HTTP/HttpRequestParser.hpp"
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
    CGI *activeCgi;
    ClientState state;
    size_t writeOffset;
    bool closeAfterWrite;
    std::string readBuffer;
    std::string writeBuffer;
    HttpRequestParser requestParser;
    const ServerConfig *activeConfig;
    const std::vector<ServerConfig *> configs;


    bool readFromSocket();
    void processReadBuffer();
    void closeConnection();
    void errorsHandler(int errorCode);
    void consumeReadBuffer(size_t bytes);
    void appendToWriteBuffer(const std::string &data);
    void appendToReadBuffer(const char *data, size_t size);
    void startCgi(const HttpRequest &request, const HttpResult &result);

    public:
    time_t timeout;

    ~Client();
    Client(int fd, Server *srv, const std::vector<ServerConfig *> &confs);
    
    
    int getFD() const;
    bool isConnected() const;
    bool hasPendingWrite() const;

    
    ClientState getState() const;
    HttpRequest &getActiveRequest();
    const std::string &getReadBuffer() const;
    const std::string &getWriteBuffer() const;

    void handleRead();
    void handleWrite();
    void terminateCgi();
    void handleEvent(int, uint32_t);
    void onCgiDone(HttpResponse &response);
};

#endif
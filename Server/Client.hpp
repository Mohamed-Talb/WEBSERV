#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <ctime>
#include <vector>
#include <stdint.h>

#include "../HTTP/RouteMatch.hpp"
#include "../HTTP/HttpHandler.hpp"
#include "../HTTP/HttpResponse.hpp"
#include "../configParser/configParser.hpp"
#include "../HTTP/HttpRequest/HttpRequest.hpp"
#include "../HTTP/HttpRequest/RequestParser.hpp"

#include <ctime>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>

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
    RequestParser requestParser;



    const ServerConfig *activeConfig;
    const std::vector<ServerConfig *> configs;


    private:
    bool readFromSocket();
    void closeConnection();
    void processReadBuffer();
    void errorsHandler(int errorCode);
    void consumeReadBuffer(size_t bytes);
    void appendToWriteBuffer(const std::string &data);
    void appendToReadBuffer(const char *data, size_t size);

    public:
    time_t lastAction;

    ~Client();
    Client(int fd, Server *srv, const std::vector<ServerConfig *> &confs);


    void handleRead();
    void handleWrite();
    void handleEvent(int, uint32_t);
    
    bool isConnected() const;
    bool hasPendingWrite() const;
    

    int getFD() const;
    ClientState getState() const;
    HttpRequest &getActiveRequest();
    const std::string &getReadBuffer() const;
    const std::string &getWriteBuffer() const;


    void terminateCgi();
    void onCgiDone(HttpResponse &response);
    void startCgi(HttpRequest &request, const HttpResult &result);
};

#endif
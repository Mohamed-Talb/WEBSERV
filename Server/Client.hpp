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

    const std::vector<ServerConfig *> configs;
    const ServerConfig *activeConfig;

    std::string readBuffer;
    std::string writeBuffer;

    RequestParser requestParser;

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
    void appendToWriteBuffer(const std::string &data);
    void appendToReadBuffer(const char *data, size_t size);

    public:
    time_t lastAction;

    Client(int fd, Server *srv, const std::vector<ServerConfig *> &confs);
    ~Client();
    int getFD() const;
    void handleRead();
    void handleWrite();
    void handleEvent(int, uint32_t);

    bool isConnected() const;
    bool hasPendingWrite() const;

    HttpRequest &getActiveRequest();

    ClientState getState() const;
    const std::string &getReadBuffer() const;
    const std::string &getWriteBuffer() const;

    void terminateCgi();
    void onCgiDone(HttpResponse &response);
    void startCgi(HttpRequest &request, const HttpResult &result);
};

#endif
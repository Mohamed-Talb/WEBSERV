
#include <map>
#include <ctime>
#include <vector>
#include <string>
#include <stdint.h>

#include "../Errors.hpp"
#include "../Lib.hpp"
#include "Client.hpp"
#include "Listener.hpp"
#include "../Helpers.hpp"
#include "../HTTP/HttpHandler.hpp"
#include "../configParser/configParser.hpp"
#include "Server.ipp"

#define TIMEOUT_DURATION 30

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
    void eventLoop();
    void init(const std::vector<ServerConfig>& confs);
    void removeHandler(int fd, bool deleteMemory = true);
    void addHandler(IEventHandler* handler, uint32_t events);
    void modifyHandler(IEventHandler* handler, uint32_t events);

    const std::vector<ServerConfig>& getConfigs() const;
};

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
    HttpRequest activeRequest; 
    std::string readBuffer;
    std::string writeBuffer;
    std::vector<ServerConfig> configs;
    CGI *activeCgi;
    const ServerConfig *activeConfig;
    const ServerConfig* matchConfig(const std::string& host) const;
    void errorsHandler(int errorCode);
    void processRequestHeaders();
    void executeRequest();
    bool readingFromSocket();
    public:
    time_t timeout;
    ClientState state;

    virtual ~Client();
    Client(int fd, Server* srv, const std::vector<ServerConfig>& confs);

    bool isConnected() const;
    void onCgiDone(HttpResponse response);
    void terminateCgi();

    bool hasPendingWrite() const;
    void consumeReadBuffer(size_t bytes);
    void consumeWriteBuffer(size_t bytes);
    void appendToWriteBuffer(const std::string& data);
    void appendToReadBuffer(const char* data, size_t size);
    
    virtual void handleRead();
    virtual void handleWrite();

    HttpRequest &getActiveRequest();
    virtual int getFD() const;
    const std::string &getReadBuffer() const;
    const std::string &getWriteBuffer() const;

};



#include "Server.hpp"
#include "../CGI/CGI.hpp"
#include "../HTTP/HttpUtils/HttpUtils.hpp"
#include "../HTTP/HttpHandler.hpp"
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <iostream>
#include <cstdlib>
#include <ctime>

Client::Client(int fd, Server *srv, const std::vector<ServerConfig> &confs) 
    : socketFD(fd), server(srv), configs(confs), state(READING_REQUEST)
{
    timeout = time(NULL);
    activeCgi = NULL;   
    activeConfig = NULL; 
}

Client::~Client()
{
    if (socketFD >= 0)
    {
        ::close(socketFD);
        socketFD = -1;
    }
    if (activeCgi)
    {
        server->removeHandler(activeCgi->getFD(), false); 
        delete activeCgi; // erase the handler for cgi too
        activeCgi = NULL;
    }
}

void Client::consumeReadBuffer(size_t bytes)
{
    if (bytes >= readBuffer.size())
        readBuffer.clear();
    else
        readBuffer.erase(0, bytes);
}

void Client::consumeWriteBuffer(size_t bytes)
{
    if (bytes >= writeBuffer.size())
        writeBuffer.clear();
    else
        writeBuffer.erase(0, bytes);
}

void Client::appendToWriteBuffer(const std::string& data) { writeBuffer += data; }
void Client::appendToReadBuffer(const char *data, size_t size) { readBuffer.append(data, size); }

int Client::getFD() const { return socketFD; }
HttpRequest &Client::getActiveRequest() { return activeRequest;}
bool Client::isConnected() const { return socketFD >= 0; }
bool Client::hasPendingWrite() const { return !writeBuffer.empty(); }
const std::string &Client::getReadBuffer() const { return readBuffer; }
const std::string &Client::getWriteBuffer() const { return writeBuffer; }

const ServerConfig *Client::matchConfig(const std::string& rawHost) const
{
    std::string host = rawHost;
    if (!host.empty() && host[host.size() - 1] == '\r') {
        host.erase(host.size() - 1);
    }
    size_t portSep = host.find(':');
    if (portSep != std::string::npos) 
        host = host.substr(0, portSep);
    
    host = toLower(host);

    for (size_t i = 0; i < configs.size(); ++i) 
    {
        for (size_t j = 0; j < configs[i].serverNames.size(); ++j) 
        {
            if (toLower(configs[i].serverNames[j]) == host) 
                return &configs[i];
        }
    }
    return &configs[0]; 
}

void Client::terminateCgi()
{
    activeCgi->killCgi();
    const ServerConfig& currConfig = (activeConfig) ? *activeConfig : configs[0];
    onCgiDone(ErrorPage(504, currConfig));
}

void Client::onCgiDone(HttpResponse response)
{
    appendToWriteBuffer(response.toString());
    activeRequest.reset();
    readBuffer.clear();
    state = SENDING_RESPONSE;
    
    if (activeCgi) 
    {
        server->removeHandler(activeCgi->getFD(), false); 
        delete activeCgi; // destroying the cgi object this function was called from??!?! 
        activeCgi = NULL;
    }
    server->modifyHandler(this, EPOLLOUT);
}

void Client::errorsHandler(int errorCode)
{
    const ServerConfig& currConfig = (activeConfig) ? *activeConfig : configs[0];
    HttpResponse response = ErrorPage(errorCode,currConfig);
    response.setHeader("Connection", "close");
    appendToWriteBuffer(response.toString());
    state = SENDING_RESPONSE;
    server->modifyHandler(this, EPOLLOUT);
    activeRequest.reset();
    readBuffer.clear(); 
}

// const size_t ABSOLUTE_MAX_BUFFER = 10 * 1024 * 1024;

bool Client::readingFromSocket()
{
    char buf[8192];
    bool dataRead = false;

    while (true)
    {
        // if (readBuffer.size() > ABSOLUTE_MAX_BUFFER)
        //     break ;
        std::cout << "ifhellooooooo\n";
        ssize_t bytes = recv(socketFD, buf, sizeof(buf), 0);
        std::cout << "hellooooooo\n";
        if (bytes == 0)
        {
            server->removeHandler(socketFD);
            return false; 
        }
        if (bytes < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            server->removeHandler(socketFD);
            return false;
        }
        appendToReadBuffer(buf, static_cast<size_t>(bytes));
        dataRead = true;
        timeout = time(NULL);
    }
    return dataRead;
}

void Client::processRequestHeaders()
{
    const ServerConfig *selectedConfig = matchConfig(activeRequest.getHeader("host"));
    activeConfig = selectedConfig ? selectedConfig : &configs[0];
    activeRequest.setMaxBodySize(activeConfig->client_max_body_size);
    std::string cl = activeRequest.getHeader("content-length");
    if (!cl.empty() && myStold(cl) > activeConfig->client_max_body_size)
    {
        errorsHandler(413);
    }
}

void Client::executeRequest()
{
    HttpHandler handler(*activeConfig);
    HttpResult result = handler.process(activeRequest);
    if (result.type == HTTP_RESULT_CGI)
    {
        state = PROCESSING_CGI;
        activeCgi = new CGI(this, server, activeRequest, *result.cgiLocation, result.cgiRequestPath);
        server->addHandler(activeCgi, EPOLLOUT); 
    }
    else 
    {
        appendToWriteBuffer(result.response.toString());
        state = SENDING_RESPONSE;
    }
    consumeReadBuffer(activeRequest.getParsedSize());
    activeRequest.reset();
}

void Client::handleRead()
{
    if (state == PROCESSING_CGI || !readingFromSocket())
        return;
    while (true)
    {
        int parseStatus = activeRequest.parse(readBuffer);
        if (activeRequest.getErrorCode() != 0) 
        {
            errorsHandler(activeRequest.getErrorCode());
            return;
        }
        if (parseStatus == 2)
        {
            processRequestHeaders();
            if (state == SENDING_RESPONSE) 
                return;
            continue;
        }
        if (parseStatus == 0) 
        {
            if (activeRequest.getParsedSize() > 0) 
            {
                consumeReadBuffer(activeRequest.getParsedSize());
                activeRequest.setParsedSize(0);
            }
            break; 
        }
        executeRequest();
        if (state == PROCESSING_CGI) 
            break;
    }
    if (hasPendingWrite() && state != PROCESSING_CGI)
    {
        if (state == SENDING_RESPONSE)
            server->modifyHandler(this, EPOLLOUT);
        else
            server->modifyHandler(this, EPOLLIN | EPOLLOUT);
    }
}

void Client::handleWrite()
{
    while (hasPendingWrite())
    {
        ssize_t bytes = send(socketFD, writeBuffer.c_str(), writeBuffer.size(), 0);
        if (bytes < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK) return;
            server->removeHandler(socketFD);
            return;
        }
        if (bytes == 0) return;
        consumeWriteBuffer(static_cast<size_t>(bytes));
        timeout = time(NULL);
    }
    if (!hasPendingWrite())
    {
        if (state == SENDING_RESPONSE || activeRequest.getHeader("Connection") == "close")
        {
             server->removeHandler(socketFD);
             return;
        }
        state = READING_REQUEST;
        server->modifyHandler(this, EPOLLIN);
    }
}



#include "Listener.hpp" 
#include <cctype>
#include <fstream>

Listener::Listener(const std::vector<ServerConfig> &confs, Server *srv) : socketFD(-1), server(srv), configs(confs)
{
    ServerConfig conf = configs[0];
    socketFD = socket(AF_INET, SOCK_STREAM, 0);
    if (socketFD < 0)
        throw ServerException("Listener", "socket() failed on port " + intToString(conf.port));

    int opt = 1;
    if (setsockopt(socketFD, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        ::close(socketFD);
        socketFD = -1;
        throw ServerException("Listener", "setsockopt() failed on port " + intToString(conf.port));
    }
    if (fcntl(socketFD, F_SETFL, O_NONBLOCK) < 0)
    {
        ::close(socketFD);
        socketFD = -1;
        throw ServerException("Listener", "fcntl() failed on port " + intToString(conf.port));
    }
    sockaddr_in addr;
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(conf.port);
    addr.sin_addr.s_addr = inet_addr(conf.host.c_str()); 

    if (addr.sin_addr.s_addr == INADDR_NONE)
        addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(socketFD, (sockaddr*)&addr, sizeof(addr)) < 0)
    {
        ::close(socketFD);
        socketFD = -1;
        throw ServerException("Listener", "bind() failed on port " + intToString(conf.port));
    }
    if (listen(socketFD, SOMAXCONN) < 0)
    {
        ::close(socketFD);
        socketFD = -1;
        throw ServerException("Listener", "listen() failed on port " + intToString(conf.port));
    }
}

Listener::~Listener()
{
    if (socketFD >= 0)
    {
        ::close(socketFD);
        socketFD = -1;
    }
}

void Listener::handleRead()
{
    while (true)
    {
        int clientFD = accept(socketFD, NULL, NULL);
        if (clientFD < 0)
        {
            break; 
        }
        if (fcntl(clientFD, F_SETFL, O_NONBLOCK) < 0)
        {
            ::close(clientFD);
            continue;
        }
        Client* newClient = new Client(clientFD, server, configs);
        server->addHandler(newClient, EPOLLIN); 
    }
}

void Listener::handleWrite() {}
int Listener::getFD() const {return socketFD;}
int Listener::getPort() const {return configs[0].port;}



#include "Server.hpp"
#include <sys/epoll.h>
#include <unistd.h>
#include <stdexcept>

Server::Server() : epollFD(-1) {}

Server::~Server()
{
    std::map<int, IEventHandler*>::iterator it;
    for (it = fdHandlers.begin(); it != fdHandlers.end(); ++it)
    {
        delete it->second;
    }
    fdHandlers.clear();
    if (epollFD >= 0)
    {
        ::close(epollFD);
        epollFD = -1;
    }
}

void Server::init(const std::vector<ServerConfig> &confs)
{
    configs = confs;
    epollFD = epoll_create(1000);
    if (epollFD < 0)
        throw ServerException("Server", "epoll_create failed");
    
    std::map<std::string, std::vector<ServerConfig> > groupedConfigs;
    for (size_t i = 0; i < configs.size(); ++i) 
    {
        std::string ipPort = configs[i].host + ":" + intToString(configs[i].port);
        groupedConfigs[ipPort].push_back(configs[i]);
    }
    std::map<std::string, std::vector<ServerConfig> >::iterator it;
    for (it = groupedConfigs.begin(); it != groupedConfigs.end(); ++it)
    {
        Listener* listener = new Listener(it->second, this); 
        addHandler(listener, EPOLLIN);
    }
}

void Server::addHandler(IEventHandler *handler, uint32_t events)
{
    int fd = handler->getFD();
    fdHandlers[fd] = handler;
    
    epoll_event ev;
    ev.events = events;
    ev.data.fd = fd;
    epoll_ctl(epollFD, EPOLL_CTL_ADD, fd, &ev);
}

void Server::modifyHandler(IEventHandler *handler, uint32_t events)
{
    epoll_event ev;
    ev.events = events;
    ev.data.fd = handler->getFD();
    epoll_ctl(epollFD, EPOLL_CTL_MOD, handler->getFD(), &ev);
}

const std::vector<ServerConfig>& Server::getConfigs() const
{
    return configs;
}

void Server::checkTimeout()
{
    time_t curr_time = time(NULL);
    std::vector<Client *> expiredClients;
    std::vector<Client *> cgiTimeoutClients;

    for (std::map<int, IEventHandler *>::iterator it = fdHandlers.begin(); it != fdHandlers.end(); it++)
    {
        Client *client = dynamic_cast<Client *>(it->second);
        if (client != NULL && difftime(curr_time, client->timeout) > TIMEOUT_DURATION)
        {
            if (client->state != PROCESSING_CGI)
                expiredClients.push_back(client);
            else
                cgiTimeoutClients.push_back(client);
        }
    }

    // client timeout
    for (size_t i = 0; i < expiredClients.size(); i++)
        removeHandler(expiredClients[i]->getFD());

    // cgi timeout
    for (size_t i = 0; i < cgiTimeoutClients.size(); i++)
    {
        Client *client = cgiTimeoutClients[i];
        client->terminateCgi();
    }
}

void Server::removeHandler(int fd, bool deleteMemory)
{
    epoll_ctl(epollFD, EPOLL_CTL_DEL, fd, NULL);
    std::map<int, IEventHandler*>::iterator it = fdHandlers.find(fd);
    if (it != fdHandlers.end())
    {
        if (deleteMemory) 
        {
            delete it->second;
        }
        fdHandlers.erase(it);
    }
}

void Server::eventLoop()
{
    const int MAX_EVENTS = 1024;
    epoll_event readyEvents[MAX_EVENTS];

    while (true)
    {
        int ready = epoll_wait(epollFD, readyEvents, MAX_EVENTS, 1000);
        if (ready == -1)
        {
            if (errno == EINTR) // os just wanted us to check a signal
                continue;
            break; // To-Do: unrecoverable error, clean up resources and inform the user using perror
        }
        for (int i = 0; i < ready; ++i)
        {
            int fd = readyEvents[i].data.fd;
            uint32_t currEvent = readyEvents[i].events;

            if (fdHandlers.find(fd) == fdHandlers.end())
                continue;
            IEventHandler* handler = fdHandlers[fd];
            if (currEvent & (EPOLLERR | EPOLLHUP))
            {
                currEvent |= EPOLLIN | EPOLLOUT;
            }
            if (currEvent & EPOLLIN)
            {
                handler->handleRead();
                if (fdHandlers.find(fd) == fdHandlers.end())
                    continue;
            }
            else if (currEvent & EPOLLOUT)
            {
                handler->handleWrite();
            }
        }
        checkTimeout();
    }
    // cleanup funtion needed here for when we break off the loop
}

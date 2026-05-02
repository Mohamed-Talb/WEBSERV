#include "Server.hpp"
#include "../CGI/CGI.hpp"
#include "../HTTP/HttpUtils.hpp"
#include "../HTTP/HttpHandler.hpp"
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <iostream>
#include <cstdlib>

Client::Client(int fd, Server *srv, const std::vector<ServerConfig> &confs) 
    : socketFD(fd), server(srv), configs(confs), state(READING_REQUEST) {}

Client::~Client()
{
    if (socketFD >= 0)
    {
        ::close(socketFD);
        socketFD = -1;
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
HttpRequest &Client::getRequest() { return request; }
bool Client::isConnected() const { return socketFD >= 0; }
bool Client::hasPendingWrite() const { return !writeBuffer.empty(); }
const std::string &Client::getReadBuffer() const { return readBuffer; }
const std::string &Client::getWriteBuffer() const { return writeBuffer; }

const ServerConfig *Client::matchConfig(const std::string& rawHost) const
{
    std::string host = rawHost;
    size_t portSep = host.find(':');
    if (portSep != std::string::npos) 
        host = host.substr(0, portSep);
    
    host = toLower(host);

    for (size_t i = 0; i < configs.size(); ++i) 
    {
        for (size_t j = 0; j < configs[i].serverName.size(); ++j) 
        {
            if (configs[i].serverName[j] == host) 
                return &configs[i];
        }
    }
    return &configs[0]; // ?????????? mtaleb: fix the bug here 
}

void Client::onCgiDone(HttpResponse response)
{
    appendToWriteBuffer(response.toString());
    state = SENDING_RESPONSE;
    server->modifyHandler(this, EPOLLIN | EPOLLOUT); 
}


void Client::handleRead()
{
    if (state == PROCESSING_CGI) 
        return;
    char buf[8192];
    bool dataRead = false;
    
    while (true)
    {
        int bytes = recv(socketFD, buf, sizeof(buf), 0);
        if (bytes == 0)
        {
            server->removeHandler(socketFD);
            return;
        }
        if (bytes < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            server->removeHandler(socketFD);
            return;
        }
        appendToReadBuffer(buf, bytes);
        dataRead = true;
    }
    std::cout << readBuffer << std::endl;
    if (!dataRead)
        return;
    while (true)
    {
        int parseStatus = request.parse(readBuffer);
        if (request.getErrorCode() != 0)
        {
            HttpResponse response = HttpUtils::ErrorPage(request.getErrorCode(), "Bad Request", configs[0]);
            appendToWriteBuffer(response.toString());
            state = SENDING_RESPONSE;
            server->modifyHandler(this, EPOLLIN | EPOLLOUT);
            request.reset();
            return; 
        }
		if (request.getState() == PARSE_BODY || request.getState() == PARSE_COMPLETE)
		{
			const ServerConfig *selectedConfig = matchConfig(request.getHeader("host"));
			if (!selectedConfig)
				selectedConfig = &configs[0];
			std::string cl = request.getHeader("content-length");
			if (!cl.empty())
			{
				ssize_t bodySize = myStold(cl);
				if (bodySize > selectedConfig->client_max_body_size)
				{
					HttpResponse err = HttpUtils::ErrorPage(413,"Payload Too Large",*selectedConfig);
					appendToWriteBuffer(err.toString());
					state = SENDING_RESPONSE;
					server->modifyHandler(this, EPOLLIN | EPOLLOUT);
					request.reset();
					readBuffer.clear();
					return ;
				}
			}
		}
        if (parseStatus == 0)
            break;
        const ServerConfig *selectedConfig = matchConfig(request.getHeader("host"));
        if (!selectedConfig) selectedConfig = &configs[0];
		HttpHandler handler(*selectedConfig); 

        const Location* cgiLocation = handler.getCgiLocation(request);
        if (cgiLocation != NULL) 
        {
            state = PROCESSING_CGI; 
            
            std::string requestPath = HttpUtils::stripQuery(request.getTarget());
            CgiHandler* Cgi = new CgiHandler(this, server, request, *cgiLocation, requestPath);
            server->addHandler(Cgi, EPOLLIN);
            
            consumeReadBuffer(request.getParsedSize());
            request.reset();
            break; 
        }
        else 
        {
            HttpResponse response = handler.process(request);
            appendToWriteBuffer(response.toString());
            state = SENDING_RESPONSE;
            consumeReadBuffer(request.getParsedSize());
            request.reset(); 
        }
    }
    if (hasPendingWrite() && state != PROCESSING_CGI)
        server->modifyHandler(this, EPOLLIN | EPOLLOUT);
}

void Client::handleWrite()
{
    if (!hasPendingWrite()) return;
    
    // std::cout << "RESPONSE---------------\n" << writeBuffer << std::endl;
    
    while (hasPendingWrite())
    {
        int bytes = send(socketFD, writeBuffer.c_str(), writeBuffer.size(), 0);
        if (bytes < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return;
            server->removeHandler(socketFD);
            return;
        }
        consumeWriteBuffer(bytes);
    }
    if (!hasPendingWrite())
    {
        if (request.getErrorCode() != 0) 
        {
            server->removeHandler(socketFD);
            return;
        }
        state = READING_REQUEST; 
        server->modifyHandler(this, EPOLLIN);
    }
}



#include "Server.hpp" 
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
    epollFD = epoll_create(1024);
    if (epollFD < 0)
        throw ServerException("Server", "epoll_create failed");
    
    std::map<int, std::vector<ServerConfig> > groupedConfigs;
    for (size_t i = 0; i < configs.size(); ++i) 
    {
        groupedConfigs[configs[i].port].push_back(configs[i]);
    }
    std::map<int, std::vector<ServerConfig> >::iterator it;
    for (it = groupedConfigs.begin(); it != groupedConfigs.end(); ++it)
    {
        Listener* listener = new Listener(it->second, this); 
        addHandler(listener, EPOLLIN);
    }
}

void Server::addHandler(IEventHandler* handler, uint32_t events)
{
    int fd = handler->getFD();
    fdHandlers[fd] = handler;
    
    epoll_event ev;
    ev.events = events;
    ev.data.fd = fd;
    epoll_ctl(epollFD, EPOLL_CTL_ADD, fd, &ev);
}

void Server::modifyHandler(IEventHandler* handler, uint32_t events)
{
    epoll_event ev;
    ev.events = events;
    ev.data.fd = handler->getFD();
    epoll_ctl(epollFD, EPOLL_CTL_MOD, handler->getFD(), &ev);
}

void Server::removeHandler(int fd)
{
    epoll_ctl(epollFD, EPOLL_CTL_DEL, fd, NULL);
    std::map<int, IEventHandler*>::iterator it = fdHandlers.find(fd);
    if (it != fdHandlers.end())
    {
        delete it->second;
        fdHandlers.erase(it);
    }
    ::close(fd);
}

const std::vector<ServerConfig>& Server::getConfigs() const
{
    return configs;
}

void Server::runEventLoop()
{
    const int MAX_EVENTS = 1024;
    epoll_event readyEvents[MAX_EVENTS];

    while (true)
    {
        int ready = epoll_wait(epollFD, readyEvents, MAX_EVENTS, -1);
        
        for (int i = 0; i < ready; ++i)
        {
            int fd = readyEvents[i].data.fd;
            uint32_t currEvent = readyEvents[i].events;

            if (fdHandlers.find(fd) == fdHandlers.end())
                continue;
            IEventHandler* handler = fdHandlers[fd];
            if (currEvent & (EPOLLERR | EPOLLHUP))
            {
                removeHandler(fd);
                continue;
            }
            if (currEvent & EPOLLIN)
            {
                handler->handleRead();
            }
            if (currEvent & EPOLLOUT)
                handler->handleWrite();
        }
    }
}
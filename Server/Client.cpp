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
        if (parseStatus == 0)
            break;

        const ServerConfig *selectedConfig = matchConfig(request.getHeader("host"));
        if (!selectedConfig) selectedConfig = &configs[0];
        std::string cl = request.getHeader("Content-Length");
		if (!cl.empty())
		{
			ssize_t bodySize = myStoul(cl);
			if (bodySize < 0 || bodySize > selectedConfig->client_max_body_size)
			{
				HttpResponse err = HttpUtils::ErrorPage(413, "Payload Too Large", *selectedConfig);
				appendToWriteBuffer(err.toString());
				state = SENDING_RESPONSE;
				server->modifyHandler(this, EPOLLIN | EPOLLOUT);
				request.reset();
				return;
			}
		}
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
    
    std::cout << "RESPONSE---------------\n" << writeBuffer << std::endl;
    
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
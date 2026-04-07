#include "Server.hpp"


Client::Client() : socketFd(-1), serverFd(-1) {}


Client::Client(int clientFd, int serverFd)
    : socketFd(clientFd), serverFd(serverFd) {}


Client::Client(const Client& other)
{
    socketFd = other.socketFd;
    serverFd = other.serverFd;
    bufferRead = other.bufferRead;
    bufferWrite = other.bufferWrite;
}

Client& Client::operator=(const Client& other)
{
    if (this != &other)
    {
        socketFd = other.socketFd;
        serverFd = other.serverFd;
        bufferRead = other.bufferRead;
        bufferWrite = other.bufferWrite;
    }
    return *this;
}


Client::~Client()
{
    // ?????????????????????????
}
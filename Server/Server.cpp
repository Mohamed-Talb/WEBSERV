#include "Server.hpp"

Server::Server() : socketFd(-1), port(0) {}

Server::Server(int port) : socketFd(-1), port(port) {}

Server::Server(const Server& other)
{
    socketFd = other.socketFd;
    port = other.port;
    clients = other.clients; // shallow copy
}

Server& Server::operator=(const Server& other)
{
    if (this != &other)
    {
        socketFd = other.socketFd;
        port = other.port;
        clients = other.clients;
    }
    return *this;
}

Server::~Server()
{
    //??????????????????????
}
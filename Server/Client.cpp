#include "Server.hpp"

Client::Client(int fd) : socketFD(fd) {}

Client::~Client()
{
    closeConnection();
}

void Client::closeConnection()
{
    if (socketFD >= 0)
    {
        ::close(socketFD);
        socketFD = -1;
    }
}

bool Client::isConnected() const
{
    return socketFD >= 0;
}

void Client::appendToReadBuffer(const std::string& data)
{
    readBuffer += data;
}

const std::string& Client::getReadBuffer() const
{
    return readBuffer;
}

void Client::clearReadBuffer()
{
    readBuffer.clear();
}


void Client::appendToWriteBuffer(const std::string& data)
{
    writeBuffer += data;
}

const std::string& Client::getWriteBuffer() const
{
    return writeBuffer;
}

void Client::consumeWriteBuffer(size_t bytes)
{
    writeBuffer.erase(0, bytes);
}

bool Client::hasPendingWrite() const
{
    return !writeBuffer.empty();
}

int Client::getSocketFD() const
{
    return socketFD;
}
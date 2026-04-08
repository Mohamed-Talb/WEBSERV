#include "Server.hpp"

Client::Client() : socketFD(-1) {}

Client::Client(int fd) : socketFD(fd)
{
    if (fcntl(socketFD, F_SETFL, O_NONBLOCK) < 0)
    {
        return ;
        cout << "FCNTL FAILD";
        closeConnection();
        // HANDLING ERROR
    }
}


Client::~Client()
{
    if (socketFD >= 0)
        ::close(socketFD);
}

void Client::closeConnection()
{
    if (socketFD >= 0)
    {
        ::close(socketFD);
        socketFD = -1;
    }
}

int Client::getSocketFD() const
{
    return socketFD;
}

std::string& Client::getReadBuffer()
{
    return readBuffer;
}

std::string& Client::getWriteBuffer()
{
    return writeBuffer;
}

void Client::appendToReadBuffer(const std::string& data)
{
    readBuffer += data;
}

void Client::appendToWriteBuffer(const std::string& data)
{
    writeBuffer += data;
}

bool Client::hasPendingWrite() const
{
    return !writeBuffer.empty();
}
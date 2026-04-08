#ifndef CLIENT_HPP
#define CLIENT_HPP

#include "../configParser/configParser.hpp"
#include "../Lib.hpp"

using namespace std;

class Client
{
private:
    int socketFD;
    string readBuffer;
    string writeBuffer;

public:
    Client();
    Client(int clientFD);
    Client(const Client& other);
    Client& operator=(const Client& other);
    ~Client();

    void closeConnection();

    int getSocketFD() const;

    void appendToReadBuffer(const string& data);
    void appendToWriteBuffer(const string& data);
    bool hasPendingWrite() const;
    std::string& getReadBuffer();
    std::string& getWriteBuffer();
};

#endif
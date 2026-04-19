// ─── Client.hpp ─────────────────────────────────────────────────────
#ifndef CLIENT_HPP
#define CLIENT_HPP

#include "../Lib.hpp"
#include "../Helpers.hpp"
#include "../HTTP/HttpRequest.hpp"

class Client
{
private:
    int         socketFD;
    std::string readBuffer;
    std::string writeBuffer;
    HttpRequest request;

    Client();
    Client(const Client&);
    Client& operator=(const Client&);

public:
    explicit Client(int fd);
    ~Client();

    void closeConnection();
    bool isConnected()      const;

    void appendToReadBuffer(const char* data, size_t size);
    void consumeReadBuffer(size_t bytes);
    const std::string getReadBuffer()                              const;
    void               clearReadBuffer();

    void               appendToWriteBuffer(const std::string& data);
    const std::string getWriteBuffer()                             const;
    void               consumeWriteBuffer(size_t bytes);
    bool               hasPendingWrite()                            const;

    HttpRequest& getRequest();
    int  getSocketFD() const;
};

#endif
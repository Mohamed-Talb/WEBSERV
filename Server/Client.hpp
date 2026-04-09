// ─── Client.hpp ─────────────────────────────────────────────────────
#ifndef CLIENT_HPP
#define CLIENT_HPP

#include "../Lib.hpp"
#include "../Helpers.hpp"

class Client
{
private:
    int         socketFD;
    std::string readBuffer;
    std::string writeBuffer;

    Client();
    Client(const Client&);
    Client& operator=(const Client&);

public:
    explicit Client(int fd);
    ~Client();

    void closeConnection();
    bool isConnected()      const;

    void               appendToReadBuffer(const std::string& data);
    const std::string& getReadBuffer()                              const;
    void               clearReadBuffer();

    void               appendToWriteBuffer(const std::string& data);
    const std::string& getWriteBuffer()                             const;
    void               consumeWriteBuffer(size_t bytes);
    bool               hasPendingWrite()                            const;

    int  getSocketFD() const;
};

#endif
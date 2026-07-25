#ifndef SERVER_IPP
#define SERVER_IPP

#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>

class IEventHandler
{
    public:
    virtual ~IEventHandler() {}
    
    // virtual void handleRead() = 0;
    // virtual void handleWrite() = 0;
    virtual int  getFD() const = 0;
    
    virtual void handleEvent(int, uint32_t) = 0;
};

#endif
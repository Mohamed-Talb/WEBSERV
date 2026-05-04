#ifndef SERVER_IPP
#define SERVER_IPP

class IEventHandler
{
    public:
    virtual ~IEventHandler() {}

    virtual void handleRead(int fd) = 0;
    virtual void handleWrite(int fd) = 0;
};

#endif
#ifndef SERVER_IPP
#define SERVER_IPP

class IEventHandler
{
    public:
    virtual ~IEventHandler() {}
    
    virtual void handleRead() = 0;
    virtual void handleWrite() = 0;
    virtual int  getFD() const = 0; 
};

#endif
#ifndef CGIHANDLER_HPP
#define CGIHANDLER_HPP

#include "../Server/Server.ipp" 
#include "../HTTP/HttpHandler.hpp"
#include "../configParser/configParser.hpp" 
#include <string>
#include <sys/types.h>

class Client; 
class Server;

class CgiHandler : public IEventHandler
{
private:
    int         pipe_fd;
    pid_t       cgi_pid;
    std::string rawOutputBuffer;
    
    Client* parentClient;
    Server* server;
    HttpResponse parseCgiOutput(const std::string& rawOutput);

public:
    CgiHandler(Client* client, Server* srv, const HttpRequest& request, const Location& location, std::string path);
    virtual ~CgiHandler();

    virtual void handleRead();
    virtual void handleWrite();
    virtual int  getFD() const;
};

#endif
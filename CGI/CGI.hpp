#ifndef CGI_HPP
#define CGI_HPP

#include "../Server/Server.ipp" 
#include "../HTTP/HttpHandler.hpp"
#include "../HTTP/HttpResponse.hpp"
#include "../HTTP/HttpRequest.hpp"
#include "../HTTP/RouteMatch.hpp"
#include "../configParser/configParser.hpp" 
#include <string>
#include <sys/types.h>

class Client; 
class Server;

class CGI : public IEventHandler
{
    private:
    int         pipe_fd;
    pid_t       cgi_pid;
    std::string rawOutputBuffer;
    
    Server* server;
    Client* parentClient;
    HttpResponse parseCgiOutput(const std::string &rawOutput);

public:
    CGI(Client* client, Server* srv, const HttpRequest& request, const Location& location, std::string path);
    virtual ~CGI();

    virtual void handleRead();
    virtual void handleWrite();
    virtual int  getFD() const;
};

#endif
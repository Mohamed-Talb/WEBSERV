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

enum CgiState
{
    WRITING_INPUT,
    READING_OUTPUT,
    DONE
};

class CGI : public IEventHandler
{
    private:
    int pipeOutFd;         // pipeOut[0]
    int pipeInFd;          // pipeIn[1]
    pid_t cgiPid;

    size_t writeOffset;
    std::string requestBody;
    std::string headerBuffer;
    bool headersParsed;
    int statusCode;
    std::string statusReason;
    std::map<std::string, std::string> headers;

    CgiState state;

    Server* server;
    Client* parentClient;

    HttpResponse parseCgiOutput(const std::string &rawOutput);

    public:
    virtual ~CGI();
    CGI(Client* client, Server *srv, const HttpRequest &request, const Location &location, std::string path);

    void killCgi();
    virtual void handleRead();
    virtual int getFD() const;
    virtual void handleWrite();
    void freeEnv(char **envp);
    char **buildEnv(const HttpRequest &request);
};

#endif
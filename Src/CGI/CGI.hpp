#ifndef CGI_HPP
#define CGI_HPP

#include <string>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <cstring>
#include <cstdlib>
#include <sstream>
#include <errno.h>
#include <sys/types.h>
#include "../Server/Server.ipp"
#include "../HTTP/RouteMatch.hpp"
#include "../HTTP/HttpHandler.hpp"
#include "../HTTP/HttpResponse.hpp"
#include "../configParser/configParser.hpp"
#include "../HTTP/HttpRequest/HttpRequest.hpp"

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
    int pipeInFd;          // pipeIn[1]
    int pipeOutFd;         // pipeOut[0]
    pid_t cgiPid;
    
    size_t writeOffset;
    std::string requestBody;
    std::string rawOutputBuffer;
    
    CgiState state;
    
    Server* server;
    Client* parentClient;
    std::string execBin;
    
    const ServerConfig *config;

    void finish();
    void closeInput();
    void closeOutput();
    void handleInput();
    void handleOutput();
    public:
    virtual ~CGI();
    void killCgi();
    int getFD() const;
    void registerHandlers();
    void freeEnv(char **envp);
    void handleEvent(int, uint32_t);
    HttpResponse parseCgiOutput(const std::string &rawOutput);
    char **buildEnv(const HttpRequest &request, const std::string &scriptPath);
    CGI(Client *client, Server *srv, const HttpRequest &request, const std::string &fullResolvedPath, const std::string &interpreter, const ServerConfig *config);
};

#endif
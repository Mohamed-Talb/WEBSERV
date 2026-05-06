#ifndef SERVER_HPP
#define SERVER_HPP

#include <map>
#include <ctime>
#include <vector>
#include <string>
#include <stdint.h>

#include "../Lib.hpp"
#include "Server.ipp"
#include "Client.hpp"
#include "Listener.hpp"
#include "../Errors.hpp"
#include "../Helpers.hpp"
#include "../HTTP/HttpHandler.hpp"
#include "../configParser/configParser.hpp"

class Server
{
    private:
    int epollFD;
    std::vector<ServerConfig> configs;
    std::map<int, IEventHandler*> fdHandlers;

    public:
    Server();
    ~Server();

    void checkTimeout();
    void runEventLoop();
    void removeHandler(int fd, bool deleteMemory = true);
    void init(const std::vector<ServerConfig>& confs);
    void addHandler(IEventHandler* handler, uint32_t events);
    void modifyHandler(IEventHandler* handler, uint32_t events);

    const std::vector<ServerConfig>& getConfigs() const;
};


#endif
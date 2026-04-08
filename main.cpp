#include "Server/Server.hpp"

std::vector<ServerConfig> GETconfig();

int main()
{
    std::vector<ServerConfig> serverconfig = GETconfig();
    Server server;
    server.loadListeners(serverconfig);
    server.initEventSystem();
    server.runEventLoop();
}
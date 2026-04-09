#include "Server/Server.hpp"

std::vector<ServerConfig> GETconfig();
int main()
{
    try
    {
        std::vector<ServerConfig> configs = GETconfig();
        Server server;
        server.init(configs);
        server.runEventLoop();
    }
    catch (const ServerException& e)
    {
        std::cerr << e.what() << "\n";
        return 1;
    }
}
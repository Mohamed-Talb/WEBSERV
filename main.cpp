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
    catch (const std::runtime_error &e)
    {
        std::cerr << e.what() << "\n";
        return 1;
    }
}


// Timeout Management (The Slowloris Vulnerability)
// in client handlRead if the httpparser return a error custo the error page for client config
// sort the locations inside the config 
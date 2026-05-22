#include "Server/Server.hpp"

int main(int ac, char **av)
{
    signal(SIGPIPE, SIG_IGN);
    if (ac != 2)
    {
        std::cout << "Usage: ./program pathToConfig" << std::endl;
        return EXIT_FAILURE;
    }
    try
    {
        ConfigParser CP;
        std::vector<ServerConfig> configs = CP.loadeConfig(av[1]);
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

// 405 Method Not Allowed -> http://localhost:8080/upload.html;
// handl duplacated server names

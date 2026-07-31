#include "Server/Server.hpp"


int main(int ac, char **av)
{
    signal(SIGPIPE, SIG_IGN);
    srand(std::time(NULL));
    std::string configPath;
    if (ac > 2)
    {
        std::cout << "Usage: ./program pathToConfig" << std::endl;
        return EXIT_FAILURE;
    }
    else if (ac == 1)
        configPath = "configs/default.conf";
    else
        configPath = av[1];
    try
    {
        ConfigParser CP;
        std::vector<ServerConfig> configs = CP.loadeConfig(configPath);
        Server server;
        server.init(configs);
        server.eventLoop();
    }
    catch (const std::runtime_error &e)
    {
        std::cerr << e.what() << "\n";
        return 1;
    }
}

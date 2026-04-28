#include "Server/Server.hpp"

std::vector<ServerConfig> parseConfig(const std::string &configFile)
{
    std::vector<std::string> tokens = tokenize(configFile);
    ConfigParser parser(tokens);
    return parser.parse();
}

int main(int ac, char **av)
{
    (void)ac;
    try
    {
        std::vector<ServerConfig> configs = parseConfig(av[1]);
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
// 405 Method Not Allowed -> http://localhost:8080/upload.html;

#include "Server/Server.hpp"


int main(int ac, char **av)
{
    signal(SIGPIPE, SIG_IGN);
    srand(time(NULL));

    // Session session;
    // std::cout << "Session ID: " << session.getSessionId() << std::endl;
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

// 405 Method Not Allowed -> http://localhost:8080/upload.html;
// handl duplacated server names


/*
With this invalid CGI interpreter:

cgi_ext .py;
cgi_path /usr/bin/pythondd;

/usr/bin/pythondd does not exist, so the child process fails at:

execve(cgiPath.c_str(), argv, envp);

execve() returns -1 with errno == ENOENT. The Python script never starts.

Correct behavior

Inside the child process:

execve(cgiPath.c_str(), argv, envp);

std::cerr << "execve failed: " << strerror(errno) << std::endl;
_exit(127);

Do not use exit() after fork(); use _exit().

The parent later receives:

waitpid(pid, &status, WNOHANG);

and detects that the CGI exited with an error:

if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
{
    // CGI failed
}

Your Webserv should return an error response, preferably:

HTTP/1.1 502 Bad Gateway
Content-Type: text/html
Content-Length: ...
Connection: close

502 Bad Gateway is appropriate because the server could not execute the configured CGI program. 500 Internal Server Error is also acceptable for a simpler Webserv implementation, but 502 describes the failure better.
*/
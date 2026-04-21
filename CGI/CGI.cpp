#include "CGI.hpp"

std::string CgiHandler::read_all_from_pipe(int output_fd)
{
    std::string result;
    char buff;

    while (read(output_fd, &buff, 1))
    {
        result += buff;
    }
    return result;
}

std::string CgiHandler::execute(const HttpRequest& request, const Location& location, std::string path)
{
    (void) location;
    char *my_envp[3];
    my_envp[2] = NULL;
    my_envp[0] = strdup(("REQUEST_METHOD=" + request.getMethod()).c_str());
    my_envp[1] = strdup(("QUERY_STRING=" + request.getTarget().substr(request.getTarget().find("?") + 1)).c_str()); // you should free

    int pipeIn[2], pipeOut[2];
    pipe(pipeIn); pipe(pipeOut);

    pid_t pid = fork();
    if (pid == 0) {
        dup2(pipeIn[0], 0);
        dup2(pipeOut[1], 1);
        char *cgi_filepath = (char*) (path.insert(0, ".")).c_str();

        char *args[] = {(char *) "/usr/bin/python3", cgi_filepath, NULL};
        execve(args[0], args, my_envp);
        exit(1);
    }

    close(pipeIn[0]);
    close(pipeOut[1]);

    if(request.getMethod() == "POST")
    {
        write(pipeIn[1], request.getBody().c_str(), request.getBody().length());
    }
    close(pipeIn[1]);

    std::string result = read_all_from_pipe(pipeOut[0]);
    waitpid(pid, NULL, 0);
    close(pipeOut[0]);
    return result;
}
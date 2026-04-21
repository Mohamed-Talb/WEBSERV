#include "../HTTP/HttpRequest.hpp"

class CgiHandler
{
    public:
        std::string read_all_from_pipe(int output_fd);
        std::string execute(const HttpRequest& request, const Location& location, std::string path);
};

#ifndef SESSION_HPP
#define SESSION_HPP

#include <sstream>
#include <cstdlib>
#include <ctime>

class Session
{
    std::stringstream SessionId;
    public:
    Session();
    ~Session();

    void generateSessionId();
    const std::string getSessionId() const;
};

#endif
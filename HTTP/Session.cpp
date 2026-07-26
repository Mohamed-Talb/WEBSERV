#include "Session.hpp"

Session::Session()
{
    generateSessionId();
}

Session::~Session()
{
}

void Session::generateSessionId()
{
    static const char hex[] = "0123456789abcdef";

    for (int i = 0; i < 64; i++)
    {
        const int len = sizeof(hex) - 1;
        SessionId << hex[rand() % len];
    }

    SessionId.str();
}

const std::string Session::getSessionId() const
{
    return SessionId.str();
}
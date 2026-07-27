#include "Session.hpp"

#include <cstdlib>
#include <unistd.h>

Session::Session()
    : id(), data(), lastAccess(std::time(NULL))
{
}

Session::Session(const std::string &sessionId)
    : id(sessionId), data(), lastAccess(std::time(NULL))
{
}

const std::string &Session::getId() const
{
    return id;
}

bool Session::has(const std::string &key) const
{
    return data.find(key) != data.end();
}

bool Session::get(const std::string &key, std::string &value) const
{
    std::map<std::string, std::string>::const_iterator it = data.find(key);

    if (it == data.end())
        return false;

    value = it->second;
    return true;
}

void Session::set(const std::string &key, const std::string &value)
{
    data[key] = value;
    touch();
}

void Session::remove(const std::string &key)
{
    data.erase(key);
    touch();
}

void Session::clear()
{
    data.clear();
    touch();
}

void Session::touch()
{
    lastAccess = std::time(NULL);
}

bool Session::isExpired(std::time_t now, std::time_t timeout) const
{
    return std::difftime(now, lastAccess) >= timeout;
}

SessionManager::SessionManager()
{
    static bool seeded = false;

    if (!seeded)
    {
        std::srand(static_cast<unsigned int>(std::time(NULL) ^ getpid()));
        seeded = true;
    }
}

SessionManager::~SessionManager()
{
}

std::string SessionManager::generateSessionId() const
{
    static const char characters[] =
        "0123456789"
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

    const std::size_t characterCount = sizeof(characters) - 1;

    std::string sessionId;
    sessionId.reserve(32);

    for (std::size_t i = 0; i < 32; ++i)
        sessionId += characters[std::rand() % characterCount];

    return sessionId;
}

Session *SessionManager::createSession()
{
    std::string sessionId;

    do
    {
        sessionId = generateSessionId();
    }
    while (sessions.find(sessionId) != sessions.end());

    std::pair<std::map<std::string, Session>::iterator, bool> result =
        sessions.insert(std::make_pair(sessionId, Session(sessionId)));

    if (!result.second)
        return NULL;

    return &result.first->second;
}

Session *SessionManager::findSession(const std::string &sessionId)
{
    std::map<std::string, Session>::iterator it = sessions.find(sessionId);

    if (it == sessions.end())
        return NULL;

    it->second.touch();
    return &it->second;
}

bool SessionManager::removeSession(const std::string &sessionId)
{
    return sessions.erase(sessionId) != 0;
}

void SessionManager::removeExpiredSessions(std::time_t now, std::time_t timeout)
{
    std::map<std::string, Session>::iterator it = sessions.begin();

    while (it != sessions.end())
    {
        if (it->second.isExpired(now, timeout))
        {
            std::map<std::string, Session>::iterator expired = it;
            ++it;
            sessions.erase(expired);
        }
        else
        {
            ++it;
        }
    }
}

void SessionManager::clear()
{
    sessions.clear();
}

std::size_t SessionManager::size() const
{
    return sessions.size();
}
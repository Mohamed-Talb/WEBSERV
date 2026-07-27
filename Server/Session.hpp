#ifndef SESSION_HPP
#define SESSION_HPP

#include <map>
#include <string>
#include <cstddef>
#include <ctime>

struct Session
{
private:
    std::string id;
    std::map<std::string, std::string> data;
    std::time_t lastAccess;

public:
    Session();
    Session(const std::string &sessionId);

    const std::string &getId() const;

    bool has(const std::string &key) const;
    bool get(const std::string &key, std::string &value) const;

    void set(const std::string &key, const std::string &value);
    void remove(const std::string &key);
    void clear();

    void touch();
    bool isExpired(std::time_t now, std::time_t timeout) const;
};

class SessionManager
{
private:
    std::map<std::string, Session> sessions;

    std::string generateSessionId() const;

public:
    SessionManager();
    ~SessionManager();

    Session *createSession();
    Session *findSession(const std::string &sessionId);

    bool removeSession(const std::string &sessionId);
    void removeExpiredSessions(std::time_t now, std::time_t timeout);

    void clear();
    std::size_t size() const;
};

#endif
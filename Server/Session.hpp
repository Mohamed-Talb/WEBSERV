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
    std::time_t lastAccess;
    std::map<std::string, std::string> data;

    public:
    Session();
    Session(const std::string &sessionId);

    const std::string &getId() const;

    bool has(const std::string &key) const;
    bool get(const std::string &key, std::string &value) const;
    bool isExpired(std::time_t now, std::time_t timeout) const;

    void touch();
    void clear();
    void remove(const std::string &key);
    void set(const std::string &key, const std::string &value);

};

class SessionManager
{
    private:
    std::string generateSessionId() const;
    std::map<std::string, Session> sessions;

    public:
    SessionManager();
    ~SessionManager();

    Session *createSession();
    Session *findSession(const std::string &sessionId);

    void clear();
    std::size_t size() const;
    bool removeSession(const std::string &sessionId);
    void removeExpiredSessions(std::time_t now, std::time_t timeout);
};

#endif
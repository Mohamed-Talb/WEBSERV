#include "Server.hpp"

ServerManager::ServerManager() {}

ServerManager::ServerManager(const ServerManager& other)
{
    servers = other.servers; // shallow copy
}

ServerManager& ServerManager::operator=(const ServerManager& other)
{
    if (this != &other)
        servers = other.servers;

    return *this;
}

ServerManager::~ServerManager()
{
  ///????????????
}
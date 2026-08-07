#include "./HttpUtils.hpp"

const ServerConfig *matchConfig(const std::vector<ServerConfig *> &configs, const std::string &rawHost)
{
    if (configs.empty())
        return NULL;

    std::string host = rawHost;

    if (!host.empty() && host[host.size() - 1] == '\r')
        host.erase(host.size() - 1);

    size_t portSeparator = host.find(':');

    if (portSeparator != std::string::npos)
        host = host.substr(0, portSeparator);

    host = toLower(host);
    for (size_t i = 0; i < configs.size(); ++i)
    {
        for (size_t j = 0; j < configs[i]->serverNames.size(); ++j)
        {
            if (toLower(configs[i]->serverNames[j]) == host)
                return configs[i];
        }
    }
    return configs[0];
}

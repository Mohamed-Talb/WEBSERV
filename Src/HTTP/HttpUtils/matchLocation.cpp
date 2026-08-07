#include "HttpUtils.hpp"

const Location *matchLocation(const ServerConfig &config, const std::string &path)
{
    const Location *bestMatch = NULL;
    size_t bestLength = 0;

    for (size_t i = 0; i < config.locations.size(); ++i)
    {
        const Location &location = config.locations[i];

        if (path.size() < location.path.size())
            continue;

        if (path.compare(0, location.path.size(), location.path) != 0)
            continue;

        bool validBoundary = location.path == "/"
            || path.size() == location.path.size()
            || location.path[location.path.size() - 1] == '/'
            || path[location.path.size()] == '/';

        if (!validBoundary)
            continue;

        if (location.path.size() > bestLength)
        {
            bestMatch = &location;
            bestLength = location.path.size();
        }
    }

    return bestMatch;
}

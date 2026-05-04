#ifndef ROUTEMATCH_HPP
#define ROUTEMATCH_HPP

#include <string>
#include "../configParser/config.hpp"

struct RouteMatch
{
    const Location *location;
    std::string requestPath;
    std::string root;
    std::string fullPath;
};

#endif
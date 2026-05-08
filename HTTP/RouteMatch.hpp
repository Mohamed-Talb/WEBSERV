#ifndef ROUTEMATCH_HPP
#define ROUTEMATCH_HPP

#include <string>
#include "../configParser/config.hpp"

struct RouteMatch
{
    std::string root;
    const Location *location;
    std::string requestPath;
    std::string fullPath;
};

#endif
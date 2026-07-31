#ifndef ROUTEMATCH_HPP
#define ROUTEMATCH_HPP

#include <string>
#include "../configParser/configParser.hpp"

struct RouteMatch
{
    std::string root;
    std::string fullPath;
    std::string requestPath;
    const Location *location;
};

#endif
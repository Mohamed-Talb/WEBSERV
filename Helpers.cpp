#include "Helpers.hpp"

std::string intToString(int value)
{
    std::stringstream ss;
    ss << value;
    return ss.str();
}
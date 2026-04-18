#include "Helpers.hpp"

std::string intToString(int value)
{
    std::stringstream ss;
    ss << value;
    return ss.str();
}

std::string toUpper(std::string value)
{
    for (size_t i = 0; i < value.size(); ++i)
        value[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(value[i])));
    return value;
}

std::string toLower(std::string value)
{
    for (size_t i = 0; i < value.size(); ++i)
        value[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(value[i])));
    return value;
}
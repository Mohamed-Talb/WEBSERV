#ifndef ERRORS_HPP
#define ERRORS_HPP
#include <iostream>
#include <stdexcept>
#include <string>
#include <cerrno>

class ServerException : public std::runtime_error
{
    public:
    ServerException(const std::string& context, const std::string& msg);
};
void logError(const std::string& context, const std::string& msg);

#endif
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

enum ConfigErrorType 
{
    ERR_DUPLICATE_DIRECTIVE,
    ERR_DUPLICATE_VALUE,
    ERR_MISSING_VALUE,
    ERR_INVALID_VALUE,
    ERR_MISSING_SEMICOLON,
    ERR_INVALID_SYNTAX
};

void throwError(ConfigErrorType type, const std::string &target);
#endif
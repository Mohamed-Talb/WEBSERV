#ifndef HELPERS_HPP
#define HELPERS_HPP

#include <string>
#include <vector>
#include <cstdlib>
#include <sstream>
#include <sys/types.h>

int hexDigit(char c);
std::string intToString(int value);
bool        isValidErrorCode(int code);
std::string toUpper(std::string value);
std::string toLower(std::string value);
std::string trim(const std::string &value);
ssize_t     myStold(const std::string &str);
bool        isOnlyDigits(const std::string &s);
bool        isValidHost(const std::string &host);
std::string mergeSlashes(const std::string &path);
bool        isValidServerName(const std::string &name);
std::string joinPath(std::string left, std::string right);
#endif
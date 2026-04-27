#ifndef HELPERS_HPP
#define HELPERS_HPP

#include "Lib.hpp"

std::string intToString(int value);
std::string toUpper(std::string value);
std::string toLower(std::string value);
std::string trim(const std::string &value);
ssize_t myStold(const std::string &str);
std::string joinPath(std::string left, std::string right);
std::string mergeSlashes(const std::string &path);
#endif
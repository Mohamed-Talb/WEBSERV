#ifndef FILESYSTEM_HPP
#define FILESYSTEM_HPP

#include <string>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <sys/stat.h>


bool        fileExists(const std::string &filePath);
bool        readFile(const std::string &filePath, std::string& content);
bool        deleteFile(const std::string &filePath);
bool        writeToFile(const std::string &filePath, std::string &content);

#endif


#ifndef FILESYSTEM_HPP
#define FILESYSTEM_HPP

#include <string>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <sys/stat.h>


bool    isDirectory(const std::string &path);
bool    fileExists(const std::string &filePath);
bool    deleteFile(const std::string &filePath);
bool    isRegularFile(const std::string &path);
bool    readFile(const std::string &filePath, std::string& content);
bool    writeToFile(const std::string &filePath, const std::string &content);
bool isReadable(const std::string &path);
bool hasAccessDenied(const std::string &path);
bool isReadableFile(const std::string &path);
#endif


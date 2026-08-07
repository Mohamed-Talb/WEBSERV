#ifndef FILESYSTEM_HPP
#define FILESYSTEM_HPP

#include <string>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>


bool isReadable(const std::string &path);
bool isDirectory(const std::string &path);
bool isRegularFile(const std::string &path);
bool isReadableFile(const std::string &path);
bool fileExists(const std::string &filePath);
bool deleteFile(const std::string &filePath);
bool hasAccessDenied(const std::string &path);
std::string getAbsolutePath(const std::string &path);
bool readFile(const std::string &filePath, std::string& content);
bool writeToFile(const std::string &filePath, const std::string &content);
#endif


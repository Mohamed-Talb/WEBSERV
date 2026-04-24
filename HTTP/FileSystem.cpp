#include "HttpHandler.hpp" // Assuming this contains your FileSystem declaration
#include <iostream>
#include <fstream>
#include <cstdio>
#include <unistd.h>
#include <sys/stat.h>

namespace FileSystem
{
    bool fileExists(const std::string &filePath)
    {
        return (access(filePath.c_str(), F_OK) == 0);
    }
    bool readFile(const std::string& filePath, std::string& content)
    {
        std::ifstream file(filePath.c_str(), std::ios::in | std::ios::binary);
        if (!file.is_open())
            return false;
        std::ostringstream buffer;
        buffer << file.rdbuf();
        content = buffer.str();
        return true;
    }

    bool deleteFile(const std::string &filePath)
    {
        if (std::remove(filePath.c_str()) == 0)
            return true;
        return false;
    }

    bool writeToFile(const std::string &filePath, std::string &content)
    {
        std::ofstream outfile(filePath.c_str(), std::ios::out | std::ios::trunc);
        if (!outfile.is_open())
            return false;
        
        outfile << content;
        outfile.close();
        return true;
    }
}
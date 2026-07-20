#include "FileSystem.hpp"

bool isReadable(const std::string &path)
{
    return access(path.c_str(), R_OK) == 0;
}

bool isReadableFile(const std::string &path)
{
    return isRegularFile(path) && access(path.c_str(), R_OK) == 0;
}

bool hasAccessDenied(const std::string &path)
{
    struct stat info;

    errno = 0;

    if (stat(path.c_str(), &info) == 0)
        return false;

    return errno == EACCES;
}

bool fileExists(const std::string &filePath)
{
    return (access(filePath.c_str(), F_OK) == 0);
}

bool isRegularFile(const std::string &path)
{
    struct stat info;

    if (stat(path.c_str(), &info) != 0)
        return false;

    return S_ISREG(info.st_mode);
}

bool readFile(const std::string &filePath, std::string &content)
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

bool writeToFile(const std::string &filePath, const std::string &content)
{
    std::ofstream outfile(filePath.c_str(), std::ios::out | std::ios::trunc);
    if (!outfile.is_open())
        return false;
    
    outfile << content;
    outfile.close();
    return true;
}

bool isDirectory(const std::string &path)
{
    struct stat s;
    if (stat(path.c_str(), &s) != 0)
        return false;
    return S_ISDIR(s.st_mode);
}

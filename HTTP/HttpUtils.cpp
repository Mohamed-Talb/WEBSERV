#include "HttpHandler.hpp"
#include <fstream>
#include <sstream>
#include <map>
#include "../Helpers.hpp"

namespace HttpUtils 
{

    std::string contentType(const std::string& path)
    {
        static std::map<std::string, std::string> mimeTypes;
        if (mimeTypes.empty())
        {
            mimeTypes.insert(std::make_pair(".html", "text/html"));
            mimeTypes.insert(std::make_pair(".htm", "text/html"));
            mimeTypes.insert(std::make_pair(".css", "text/css"));
            mimeTypes.insert(std::make_pair(".js", "application/javascript"));
            mimeTypes.insert(std::make_pair(".json", "application/json"));
            mimeTypes.insert(std::make_pair(".txt", "text/plain"));
            mimeTypes.insert(std::make_pair(".png", "image/png"));
            mimeTypes.insert(std::make_pair(".jpg", "image/jpeg"));
            mimeTypes.insert(std::make_pair(".jpeg", "image/jpeg"));
            mimeTypes.insert(std::make_pair(".gif", "image/gif"));
        }

        size_t pos = path.find_last_of('.');
        if (pos == std::string::npos) 
            return "application/octet-stream";
        std::string ext = toLower(path.substr(pos));
        std::map<std::string, std::string>::const_iterator it = mimeTypes.find(ext);
        if (it != mimeTypes.end()) 
            return it->second;
        return "application/octet-stream";
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

    std::string stripQuery(const std::string& path)
    {
        size_t pos = path.find('?');
        if (pos == std::string::npos) 
            return path;
        return path.substr(0, pos);
    }
}
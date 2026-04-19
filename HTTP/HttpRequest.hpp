
#ifndef HTTPREQUEST_HPP
#define HTTPREQUEST_HPP

#include <string>
#include <map>
#include <sstream>
#include <cctype>
#include "../Helpers.hpp"

class HttpRequest
{
private:
    std::string method;
    std::string target;
    std::string version;
    std::map<std::string, std::string> headers;
    std::string body;
    size_t consumedBytes;
    int errorCode;

public:
    HttpRequest();
    ~HttpRequest();
    void reset();
    int parse(const std::string& rawBuffer);

    bool parseRequestLine(std::istringstream& headerStream);
    bool parseHeaders(std::istringstream& headerStream);
    int  extractBody(const std::string& payload, size_t& consumedBody);
    
    
    
    std::string getMethod() const;
    std::string getTarget() const;
    std::string getVersion() const;
    std::string getHeader(const std::string& key) const;
    std::string getBody() const;
    size_t getConsumedBytes() const;
    int getErrorCode() const;
};

#endif

#ifndef HTTPRESPONSE_HPP
#define HTTPRESPONSE_HPP

#include <string>
#include <map>
#include <sstream>

class HttpResponse
{
private:
    int statusCode;
    std::string reasonPhrase;
    std::map<std::string, std::string> headers;
    std::string body;

public:
    HttpResponse();
    HttpResponse(int code, const std::string& reason);
    ~HttpResponse();

    void setHeader(const std::string& key, const std::string& value);
    void setBody(const std::string& content, const std::string& contentType);
    std::string toString() const;
};

#endif
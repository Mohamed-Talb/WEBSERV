#ifndef HTTPRESPONSE_HPP
#define HTTPRESPONSE_HPP

#include <map>
#include <string>
#include <sstream>
#include <fstream>
#include "../Helpers.hpp"

class HttpResponse
{
	private:
    int statusCode;
    std::string reasonPhrase;
    std::map<std::string, std::string> headers;
    std::string body;

	public:
    HttpResponse();
    ~HttpResponse();
    HttpResponse(int code, const std::string &reason);

    std::string toString() const;
    void setBody(const std::string &content);
    void writeBody(const std::string &chunk);
    bool setBodyFromFile(const std::string &filePath);
    void setHeader(const std::string &key, const std::string &value);
};

#endif
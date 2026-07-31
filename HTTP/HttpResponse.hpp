#ifndef HTTPRESPONSE_HPP
#define HTTPRESPONSE_HPP

#include <map>
#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <iostream>
#include "../Helpers.hpp"

class HttpResponse
{
    private:
        int statusCode;
        std::string body;
        std::string reasonPhrase;
        std::map<std::string, std::vector<std::string> > headers;

    public:
        HttpResponse();
        ~HttpResponse();
        HttpResponse(int code, const std::string &reason);

        std::string toString() const;
        void setBody(const std::string &content);
        void writeBody(const std::string &chunk);
        void setHeader(const std::string &key, const std::string &value);
        void addHeader(const std::string &key, const std::string &value);
};

#endif

#ifndef HTTPREQUEST_HPP
#define HTTPREQUEST_HPP
#include <map>
#include <string>
#include <cstddef>
#include <sstream>


enum State 
{
	PARSE_REQUEST_LINE,
	PARSE_HEADERS,
	PARSE_BODY,
	PARSE_COMPLETE,
	PARSE_ERROR
};


class HttpRequest 
{

	private:
    std::string method;
    std::string target;
    std::string version;
    std::map<std::string, std::string> headers;
    std::string body;
    
    State   state;
    size_t  parsedSize;
    int     errorCode;

    void setError(int code);
    int  parseBody(const std::string &raw);
    int  parseHeaders(const std::string &raw);
    int  parseRequestLine(const std::string &raw);
	public:
    HttpRequest();
    ~HttpRequest();

    void reset();
    int  parse(const std::string &rawBuffer);

    int getErrorCode() const;
    std::string getBody() const;
    size_t getParsedSize() const;
    std::string getMethod() const;
    std::string getTarget() const;
    std::string getVersion() const;
    std::string getHeader(const std::string &key) const;
    

    State getState() const { return state; }
};

#endif
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
    std::string body;
    std::string method;
    std::string target;
    std::string requestPath;
    std::string querys;
    std::string version;
    std::map<std::string, std::string> headers;
    
    State   state;
    size_t  parsedSize;
    int     errorCode;

    void spliteTarget();
    void setError(int code);
    int  parseBody(const std::string &raw);
    int  parseHeaders(const std::string &raw);
    int  parseRequestLine(const std::string &raw);
	
    public:
    HttpRequest();
    ~HttpRequest();

    void reset();
    int  parse(const std::string &rawBuffer);


    const std::string &getBody() const;
    State       getState() const;
    const std::string &getTarget() const;
    const std::string &getQuery() const;
    const std::string &getRequestPath() const;
    const std::string &getMethod() const;
    const std::string &getVersion() const;
    int         getErrorCode() const;
    size_t      getParsedSize() const;
    const std::string &getHeader(const std::string &key) const;
};

#endif
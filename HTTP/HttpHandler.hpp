
#ifndef HTTPHANDLER_HPP
#define HTTPHANDLER_HPP

#include "../configParser/configParser.hpp"
#include <string>
#include <fstream>
#include <cctype>
#include "../Helpers.hpp"
#include "algorithm"


#include <string>
#include <map>
#include <sstream>


enum State 
{
	PARSE_REQUEST_LINE,
	PARSE_HEADERS,
	PARSE_BODY,
	PARSE_COMPLETE,
	PARSE_ERROR
};

namespace HttpUtils
{
    std::string contentType(const std::string& path);
    bool        readFile(const std::string& filePath, std::string& content);
    std::string stripQuery(const std::string& path);
}

class HttpRequest 
{

	private:
    std::string method;
    std::string target;
    std::string version;
    std::map<std::string, std::string> headers;
    std::string body;
    
    State state;
    size_t consumedBytes;
    int 	errorCode;

    void setError(int code);
    int parseRequestLine(const std::string& raw);
    int parseHeaders(const std::string& raw);
    int parseBody(const std::string& raw);
	public:
    HttpRequest();
    ~HttpRequest();

    void reset();
    int parse(const std::string& rawBuffer);

    int getErrorCode() const;
    std::string getBody() const;
    std::string getMethod() const;
    std::string getTarget() const;
    std::string getVersion() const;
    size_t getConsumedBytes() const;
    std::string getHeader(const std::string& key) const;
    

    State getState() const { return state; }
};


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


class HttpHandler 
{
	private:
    const ServerConfig 	*Config;
    std::vector<Location> sortedLocations;
    
    const Location* matchLocation(const std::string& path);
    bool 			isMethodAllowed(const std::string& method, const Location& loc);
    HttpResponse 	handle404();
    HttpResponse 	buildError(int code, const std::string& reason, const std::string& detail);
	
	// METHODS
    HttpResponse 	GET(const HttpRequest& req, std::string route);
	public:
    HttpHandler(const ServerConfig &serverConfig);
    ~HttpHandler();

    HttpResponse process(const HttpRequest& req);
};

#endif
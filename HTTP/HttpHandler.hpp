
#ifndef HTTPHANDLER_HPP
#define HTTPHANDLER_HPP

#include "../configParser/configParser.hpp"
#include <string>
#include <fstream>
#include <cctype>
#include "../Helpers.hpp"



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

    std::string 	contentType(const std::string& path);
    bool 			readFile(const std::string& filePath, std::string& content);
    std::string 	stripQuery(const std::string& path);
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
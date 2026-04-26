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

namespace FileSystem
{
    bool        fileExists(const std::string &filePath);
    bool        readFile(const std::string &filePath, std::string& content);
    bool        deleteFile(const std::string &filePath);
    bool        writeToFile(const std::string &filePath, std::string &content);
}

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
    int  parseBody(const std::string &raw, ServerConfig &config);
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


class HttpResponse
{
	private:
    int statusCode;
    std::string reasonPhrase;
    std::map<std::string, std::string> headers;
    std::string body;

	public:
    HttpResponse();
    HttpResponse(int code, const std::string &reason);
    ~HttpResponse();

    void setHeader(const std::string &key, const std::string &value);
    void setBody(const std::string &content, const std::string &contentType);
    std::string toString() const;
};


class HttpHandler 
{
	private:
    const ServerConfig 	*Config;
    std::vector<Location> sortedLocations;
    
    const Location* matchLocation(const std::string &path);
    bool 			isMethodAllowed(const std::string &method, const Location& loc);
	// METHODS
    public:
	const Location *getCgiLocation(const HttpRequest &request);
    HttpHandler(const ServerConfig &serverConfig);
    ~HttpHandler();

    HttpResponse process(const HttpRequest& req);
};

#endif
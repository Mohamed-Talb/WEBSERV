// ─── Listener.cpp ───────────────────────────────────────────────────
#include "Server.hpp"
#include "Client.hpp"
#include <cctype>
#include <fstream>

namespace
{
    struct ParsedRequest
    {
        std::string method;
        std::string target;
        std::string version;
        std::map<std::string, std::string> headers;
        std::string body;
    };

    static std::string toUpper(std::string value)
    {
        for (size_t i = 0; i < value.size(); ++i)
            value[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(value[i])));
        return value;
    }

    static std::string toLower(std::string value)
    {
        for (size_t i = 0; i < value.size(); ++i)
            value[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(value[i])));
        return value;
    }

    static std::string trim(const std::string& value)
    {
        size_t start = 0;
        while (start < value.size() && (value[start] == ' ' || value[start] == '\t'))
            ++start;
        size_t end = value.size();
        while (end > start && (value[end - 1] == ' ' || value[end - 1] == '\t'))
            --end;
        return value.substr(start, end - start);
    }

    static bool parseSize(const std::string& token, size_t& value)
    {
        if (token.empty())
            return false;
        std::istringstream iss(token);
        size_t parsed = 0;
        iss >> parsed;
        if (iss.fail() || !iss.eof())
            return false;
        value = parsed;
        return true;
    }

    static bool parseChunkedBody(const std::string& rawBody, std::string& outBody, size_t& consumed)
    {
        outBody.clear();
        consumed = 0;
        size_t cursor = 0;
        while (true)
        {
            size_t lineEnd = rawBody.find("\r\n", cursor);
            if (lineEnd == std::string::npos)
                return false;
            std::string sizeLine = rawBody.substr(cursor, lineEnd - cursor);
            size_t semi = sizeLine.find(';');
            if (semi != std::string::npos)
                sizeLine = sizeLine.substr(0, semi);
            std::istringstream iss(sizeLine);
            size_t chunkSize = 0;
            iss >> std::hex >> chunkSize;
            if (iss.fail() || !iss.eof())
                return false;
            cursor = lineEnd + 2;
            if (rawBody.size() < cursor + chunkSize + 2)
                return false;
            if (chunkSize == 0)
            {
                if (rawBody.size() < cursor + 2)
                    return false;
                if (rawBody.substr(cursor, 2) != "\r\n")
                    return false;
                consumed = cursor + 2;
                return true;
            }
            outBody.append(rawBody, cursor, chunkSize);
            cursor += chunkSize;
            if (rawBody.substr(cursor, 2) != "\r\n")
                return false;
            cursor += 2;
        }
    }

    static std::string detectContentType(const std::string& path)
    {
        size_t dot = path.find_last_of('.');
        if (dot == std::string::npos)
            return "application/octet-stream";
        std::string ext = toLower(path.substr(dot));
        if (ext == ".html" || ext == ".htm")
            return "text/html";
        if (ext == ".css")
            return "text/css";
        if (ext == ".js")
            return "application/javascript";
        if (ext == ".json")
            return "application/json";
        if (ext == ".txt")
            return "text/plain";
        if (ext == ".png")
            return "image/png";
        if (ext == ".jpg" || ext == ".jpeg")
            return "image/jpeg";
        if (ext == ".gif")
            return "image/gif";
        return "application/octet-stream";
    }

    static bool readFile(const std::string& filePath, std::string& content)
    {
        std::ifstream file(filePath.c_str(), std::ios::in | std::ios::binary);
        if (!file.is_open())
            return false;
        std::ostringstream data;
        data << file.rdbuf();
        content = data.str();
        return true;
    }

    static std::string buildResponse(int statusCode, const std::string& reason, const std::string& body, const std::string& contentType)
    {
        std::ostringstream oss;
        oss << "HTTP/1.1 " << statusCode << " " << reason << "\r\n";
        oss << "Content-Length: " << body.size() << "\r\n";
        oss << "Content-Type: " << contentType << "\r\n";
        oss << "Connection: close\r\n";
        oss << "\r\n";
        oss << body;
        return oss.str();
    }

    static std::string buildTextResponse(int statusCode, const std::string& reason, const std::string& body)
    {
        return buildResponse(statusCode, reason, body, "text/plain");
    }
}

Listener::Listener(const std::vector<ServerConfig>& confs): socketFD(-1), configs(confs)
{
    loadListener(configs[0]); 
}

Listener::~Listener()
{
    closeListener();
}

void Listener::loadListener(const ServerConfig& conf)
{
    socketFD = socket(AF_INET, SOCK_STREAM, 0);
    if (socketFD < 0)
        throw ServerException("Listener","socket() failed on port " + intToString(conf.port));
    int opt = 1;
    if (setsockopt(socketFD, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        ::close(socketFD);
        socketFD = -1;
        throw ServerException("Listener",
            "setsockopt() failed on port " + intToString(conf.port));
    }

    sockaddr_in addr;
    // std::memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(conf.port);
    addr.sin_addr.s_addr = inet_addr(conf.host.c_str()); 

    if (addr.sin_addr.s_addr == INADDR_NONE)
        addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(socketFD, (sockaddr*)&addr, sizeof(addr)) < 0)
    {
        ::close(socketFD);
        socketFD = -1;
        throw ServerException("Listener","bind() failed on port " + intToString(conf.port));
    }

    if (listen(socketFD, SOMAXCONN) < 0)
    {
        ::close(socketFD);
        socketFD = -1;
        throw ServerException("Listener","listen() failed on port " + intToString(conf.port));
    }
    std::cout << "[LISTENER] : Successfully bound and listening on Port " 
              << conf.port << " (Socket FD: " << socketFD << ")" << std::endl;
}

void Listener::closeListener()
{
    for (std::map<int, Client*>::iterator it = clients.begin(); it != clients.end(); ++it)
    {
        it->second->closeConnection();
        delete it->second;
    }
    clients.clear();
    if (socketFD >= 0)
    {
        ::close(socketFD);
        socketFD = -1;
    }
}



// const ServerConfig& Listener::matchConfig(const std::string& requestedHost) const // ????????????????
// {
//     return ;
// }
const ServerConfig& Listener::matchConfig(const std::string& hostHeader) const
{
    std::string requestedHost = hostHeader;
    size_t portSep = requestedHost.find(':');
    if (portSep != std::string::npos)
        requestedHost = requestedHost.substr(0, portSep);
    for (size_t i = 0; i < configs.size(); ++i)
    {
        for (size_t j = 0; j < configs[i].serverName.size(); ++j)
        {
            if (configs[i].serverName[j] == requestedHost)
                return configs[i];
        }
    }
    return configs[0];
}




Client* Listener::acceptClient()
{
    int clientFD = accept(socketFD, NULL, NULL);
    if (clientFD < 0)
        return NULL; // EAGAIN/EWOULDBLOCK — no client ready, not an error
    if (fcntl(clientFD, F_SETFL, O_NONBLOCK) < 0)
    {
        ::close(clientFD);
        throw ServerException("Listener","fcntl() failed for new client on port " + intToString(configs[0].port));
    }
    Client* client       = new Client(clientFD);
    clients[clientFD]    = client;
    return client;
}

Client* Listener::getClient(int fd) const
{
    std::map<int, Client*>::const_iterator it = clients.find(fd);
    if (it != clients.end())
        return it->second;
    return NULL;
}

void Listener::removeClient(int clientFD)
{
    std::map<int, Client*>::iterator it = clients.find(clientFD);
    if (it == clients.end())
        return;
    it->second->closeConnection();
    delete it->second;
    clients.erase(it);
}

int Listener::getSocketFD() const { return socketFD; }
int Listener::getPort()     const { return configs[0].port; }


IOState Listener::handleClientRead(Client* client)
{
    char buf[4096];
    int clientFD = client->getSocketFD();
    int bytes = recv(clientFD, buf, sizeof(buf), 0);

    if (bytes == 0) 
        return IO_DISCONNECTED;
        
    if (bytes < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return IO_PENDING;
        return IO_DISCONNECTED;
    }
    client->appendToReadBuffer(std::string(buf, bytes));
    std::string response;
    std::string readBuffer = client->getReadBuffer();
    size_t headerEnd = readBuffer.find("\r\n\r\n");
    if (headerEnd == std::string::npos)
        return IO_PENDING;

    ParsedRequest request;
    std::string headerBlock = readBuffer.substr(0, headerEnd);
    std::istringstream headerStream(headerBlock);
    std::string requestLine;
    if (!std::getline(headerStream, requestLine))
        response = buildTextResponse(400, "Bad Request", "Malformed request line");
    else
    {
        if (!requestLine.empty() && requestLine[requestLine.size() - 1] == '\r')
            requestLine.erase(requestLine.size() - 1);
        std::istringstream rl(requestLine);
        if (!(rl >> request.method >> request.target >> request.version))
            response = buildTextResponse(400, "Bad Request", "Malformed request line");
        request.method = toUpper(request.method);
        if (response.empty() && !rl.eof())
            response = buildTextResponse(400, "Bad Request", "Malformed request line");
    }

    std::string headerLine;
    while (response.empty() && std::getline(headerStream, headerLine))
    {
        if (!headerLine.empty() && headerLine[headerLine.size() - 1] == '\r')
            headerLine.erase(headerLine.size() - 1);
        if (headerLine.empty())
            continue;
        size_t sep = headerLine.find(':');
        if (sep == std::string::npos)
        {
            response = buildTextResponse(400, "Bad Request", "Malformed headers");
            break;
        }
        std::string name = toLower(trim(headerLine.substr(0, sep)));
        std::string value = trim(headerLine.substr(sep + 1));
        request.headers[name] = value;
    }

    if (response.empty() && request.version != "HTTP/1.1" && request.version != "HTTP/1.0")
        response = buildTextResponse(505, "HTTP Version Not Supported", "Unsupported HTTP version");
    if (response.empty() && request.version == "HTTP/1.1" && request.headers.find("host") == request.headers.end())
        response = buildTextResponse(400, "Bad Request", "Host header is required");

    size_t consumedBody = 0;
    std::string payload = readBuffer.substr(headerEnd + 4);
    std::map<std::string, std::string>::const_iterator teIt = request.headers.find("transfer-encoding");
    std::map<std::string, std::string>::const_iterator clIt = request.headers.find("content-length");
    if (response.empty() && teIt != request.headers.end() && clIt != request.headers.end())
        response = buildTextResponse(400, "Bad Request", "Conflicting body headers");
    if (response.empty() && teIt != request.headers.end())
    {
        if (toLower(teIt->second) != "chunked")
            response = buildTextResponse(400, "Bad Request", "Unsupported transfer encoding");
        else if (!parseChunkedBody(payload, request.body, consumedBody))
            return IO_PENDING;
    }
    else if (response.empty() && clIt != request.headers.end())
    {
        size_t contentLength = 0;
        if (!parseSize(trim(clIt->second), contentLength))
            response = buildTextResponse(400, "Bad Request", "Invalid Content-Length");
        else
        {
            if (payload.size() < contentLength)
                return IO_PENDING;
            request.body = payload.substr(0, contentLength);
            consumedBody = contentLength;
        }
    }

    if (response.empty())
    {
        if (request.method != "GET" && request.method != "POST" && request.method != "DELETE")
            response = buildTextResponse(405, "Method Not Allowed", "Unsupported method");
        else
        {
            std::string host;
            std::map<std::string, std::string>::const_iterator hostIt = request.headers.find("host");
            if (hostIt != request.headers.end())
                host = hostIt->second;
            const ServerConfig& selectedConfig = matchConfig(host);
            std::string route = request.target;
            size_t query = route.find('?');
            if (query != std::string::npos)
                route = route.substr(0, query);

            const Location* matchedLocation = NULL;
            size_t bestLen = 0;
            for (size_t i = 0; i < selectedConfig.Locations.size(); ++i)
            {
                const Location& current = selectedConfig.Locations[i];
                if (route.compare(0, current.path.size(), current.path) == 0 && current.path.size() >= bestLen)
                {
                    matchedLocation = &current;
                    bestLen = current.path.size();
                }
            }
            if (!matchedLocation || (matchedLocation->path == "/" && route != "/"))
            {
                std::string fileContent;
                bool found = readFile("./Error.html", fileContent);
                if (found)
                    response = buildResponse(404, "Not Found", fileContent, detectContentType("./Error.html"));
                else
                response = buildTextResponse(404, "Not Found", "File not found");
            }
            else if (!matchedLocation->methods.empty())
            {
                bool allowed = false;
                for (size_t i = 0; i < matchedLocation->methods.size(); ++i)
                {
                    if (toUpper(matchedLocation->methods[i]) == request.method)
                    {
                        allowed = true;
                        break;
                    }
                }
                if (!allowed)
                    response = buildTextResponse(405, "Method Not Allowed", "Method not allowed for route");
            }
        }
    }

    if (response.empty())
    {
        std::cout << "[LISTENER] : Client FD " << client->getSocketFD()
                  << " fully received request: " << request.method << " " << request.target << std::endl;
        if (request.method == "GET")
        {
            std::string host;
            std::map<std::string, std::string>::const_iterator hostIt = request.headers.find("host");
            if (hostIt != request.headers.end())
                host = hostIt->second;
            const ServerConfig& selectedConfig = matchConfig(host);
            std::string route = request.target;
            size_t query = route.find('?');
            if (query != std::string::npos)
                route = route.substr(0, query);
            if (route.empty() || route == "/")
                route = "/index.html";
            std::string filePath = selectedConfig.root + route;
            std::string fileContent;
            bool found = readFile(filePath, fileContent);
            if (!found && route == "/index.html")
            {
                filePath = "./index.html";
                found = readFile(filePath, fileContent);
            }
            if (found)
                response = buildResponse(200, "OK", fileContent, detectContentType(filePath));
            else
                response = buildTextResponse(404, "Not Found", "File not found");
        }
        else
            response = buildTextResponse(200, "OK", "Hello, World!");
    }

    client->appendToWriteBuffer(response);
    client->clearReadBuffer();
    (void)consumedBody;
    size_t parsedBytes = headerEnd + 4 + consumedBody;
    if (readBuffer.size() > parsedBytes)
        client->appendToReadBuffer(readBuffer.substr(parsedBytes));
    return IO_READY;
}

IOState Listener::handleClientWrite(Client* client)
{
    if (!client->hasPendingWrite())
        return IO_READY;
    int clientFD = client->getSocketFD();
    const std::string& buffer = client->getWriteBuffer();
    int bytes = send(clientFD, buffer.c_str(), buffer.size(), 0);

    if (bytes < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return IO_PENDING;
        return IO_DISCONNECTED;
    }
    client->consumeWriteBuffer(bytes);
    if (!client->hasPendingWrite())
    {
        // Note form mtaleb: For HTTP/1.1 Keep-Alive, you go back to reading.  For HTTP/1.0 Connection: close, you might actually return -1 here.
        return IO_READY; 
    }
    return IO_PENDING;
}

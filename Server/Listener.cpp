// ─── Listener.cpp ───────────────────────────────────────────────────
#include "Server.hpp"
#include "Client.hpp"
#include <cctype>
#include <fstream>

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
    std::string readBuffer = client->getReadBuffer();

    HttpRequest request;
    int parseStatus = request.parse(readBuffer);

    if (parseStatus == 0)
        return IO_PENDING;

    HttpResponse response;
    HttpHandler handler;

    if (request.getErrorCode() != 0)
    {
        response = handler.process(request, configs[0]); 
    }
    else
    {
        std::cout << "[LISTENER] : Client FD " << clientFD
                  << " fully received request: " << request.getMethod() << " " << request.getTarget() << std::endl;
        
        std::string host = request.getHeader("host");
        const ServerConfig& selectedConfig = matchConfig(host);
        response = handler.process(request, selectedConfig);
    }

    client->appendToWriteBuffer(response.toString());

    size_t parsedBytes = request.getConsumedBytes();
    client->clearReadBuffer();
    if (readBuffer.size() > parsedBytes)
    {
        client->appendToReadBuffer(readBuffer.substr(parsedBytes));
    }

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

// #include "configParser/configParser.hpp"
#include "Lib.hpp"

using namespace std;


// CLIENT PART 
class Client
{
private:
    int socketFD;
    string readBuffer;
    string writeBuffer;
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;
public:
    Client();
    Client(int clientFD);
    ~Client();

    void closeConnection();

    int getSocketFD() const;

    void appendToReadBuffer(const string& data);
    void appendToWriteBuffer(const string& data);
    bool hasPendingWrite() const;
    string& getReadBuffer();
    string& getWriteBuffer();
};


class Listener
{
    private:
        int socketFD;
        ServerConfig config;
        std::map<int, Client*> clients;

    public:
        Listener();
        Listener(const ServerConfig& conf);
        ~Listener();

        void loadListener(const ServerConfig& conf);   // Create socket, bind, listen
        Client* acceptClient();                        // Accept new client
        Client* getClient(int fd);                     // Get client by FD
        void removeClient(int clientFD);               // Remove client
        void closeListener();                          // Close all clients + listening socket

        int getSocketFD() const;
        int getPort() const;

        std::map<int, Client*>& getClients();
};


class Server
{
    private:
        map<int, Listener*> listeners;
        int epollFD;
        Server(const Server&) = delete;
        Server& operator=(const Server&) = delete;

    public:
        Server();
        ~Server();

        void loadListeners(vector<ServerConfig> &ports);
        void addListener(Listener* listener);

        void initEventSystem();
        void registerListener(Listener* listener); // IMPLEMENTATION ..... ??? 
 
        void handleNewConnection(Listener* listener);
        void handleClientRead(Listener* listener, Client* client);
        void handleClientWrite(Listener* listener, Client* client);

        void runEventLoop();
        void shutdown();
};


Client::Client() : socketFD(-1) {}

Client::Client(int fd) : socketFD(fd)
{
    if (fcntl(socketFD, F_SETFL, O_NONBLOCK) < 0)
    {
        return ;
        cout << "FCNTL FAILD";
        closeConnection();
        // HANDLING ERROR
    }
}


Client::~Client()
{
    if (socketFD >= 0)
        ::close(socketFD);
}

void Client::closeConnection()
{
    if (socketFD >= 0)
    {
        ::close(socketFD);
        socketFD = -1;
    }
}

int Client::getSocketFD() const
{
    return socketFD;
}

std::string& Client::getReadBuffer()
{
    return readBuffer;
}

std::string& Client::getWriteBuffer()
{
    return writeBuffer;
}

void Client::appendToReadBuffer(const std::string& data)
{
    readBuffer += data;
}

void Client::appendToWriteBuffer(const std::string& data)
{
    writeBuffer += data;
}

bool Client::hasPendingWrite() const
{
    return !writeBuffer.empty();
}


//  LISTENER PART 

Listener::Listener() : socketFD(-1) {}

Listener::Listener(const ServerConfig& conf)
{
    loadListener(conf);
}

Listener::~Listener()
{
    closeListener();
}

void Listener::loadListener(const ServerConfig &conf)
{
    config = conf; /// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    socketFD = socket(AF_INET, SOCK_STREAM, 0);
    if (socketFD < 0)
    {
        std::cerr << "Socket creation failed\n";
        return;
    }
    int opt = 1;
    if (setsockopt(socketFD, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        perror("setsockopt SO_REUSEADDR");
        exit(1);
    }
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(config.port);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(socketFD, (sockaddr*)&addr, sizeof(addr)) < 0)
    {
        std::cerr << "Bind failed\n";
        return;
    }
    if (listen(socketFD, SOMAXCONN) < 0)
    {
        std::cerr << "Listen failed\n";
        return;
    }
}

Client* Listener::acceptClient()
{
    int clientFD = accept(socketFD, NULL, NULL);
    if (clientFD < 0)
    {
        perror("accept");
        return NULL;
    }
    fcntl(clientFD, F_SETFL, O_NONBLOCK);

    Client* client = new Client(clientFD);
    clients[clientFD] = client;
    return client;
}

Client* Listener::getClient(int fd)
{
    std::map<int, Client*>::iterator it = clients.find(fd);
    if (it != clients.end())
        return it->second;
    return NULL;
}

void Listener::removeClient(int clientFD)
{
    std::map<int, Client*>::iterator it = clients.find(clientFD);
    if (it != clients.end())
    {
        Client* client = it->second;
        client->closeConnection();
        delete client;
        clients.erase(it);
    }
}

void Listener::closeListener()
{
    for (std::map<int, Client*>::iterator it = clients.begin(); it != clients.end(); ++it)
    {
        Client* client = it->second;
        client->closeConnection();
        delete client;
    }
    clients.clear();
    if (socketFD >= 0)
    {
        ::close(socketFD);
        socketFD = -1;
    }
}

int Listener::getSocketFD() const
{
    return socketFD;
}

int Listener::getPort() const
{
    return config.port;
}

std::map<int, Client*>& Listener::getClients()
{
    return clients;
}


// SERVER PART 


Server::Server() : epollFD(-1) {}

// Server::Server(const Server& other)
// {
//     listeners = other.listeners;
// }

// Server& Server::operator=(const Server& other)
// {
//     if (this != &other)
//         listeners = other.listeners;
//     return *this;
// }

Server::~Server()
{
    shutdown();
}

void Server::loadListeners(vector<ServerConfig> &configs)
{
    for (size_t i = 0; i < configs.size(); ++i)
        addListener(new Listener(configs[i]));
}

void Server::addListener(Listener* listener)
{
    listeners[listener->getSocketFD()] = listener;
}

void Server::initEventSystem()
{
    epollFD = epoll_create(1024);
    if (epollFD < 0)
    {
        perror("epoll_create");
        exit(1);
    }

    for (map<int, Listener*>::iterator it = listeners.begin(); it != listeners.end(); ++it)
    {
        int listenerFD = it->first;
        Listener* listener = it->second;
        epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.fd = listenerFD;
        if (epoll_ctl(epollFD, EPOLL_CTL_ADD, listenerFD, &ev) < 0)
        {
            perror("epoll_ctl: add listener");
            exit(1);
        }
    }
}

void Server::handleNewConnection(Listener* listener)
{
    Client* client = listener->acceptClient();
    if (!client)
        return;

    int clientFD = client->getSocketFD();
    epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = clientFD;
    if (epoll_ctl(epollFD, EPOLL_CTL_ADD, clientFD, &ev) < 0)
    {
        perror("epoll_ctl add client");
        delete client;
    }
}


void Server::handleClientWrite(Listener* listener, Client* client)
{
    string& buffer = client->getWriteBuffer();
    if (buffer.empty())
        return;
    int bytes = send(client->getSocketFD(), buffer.c_str(), buffer.size(), 0);
    if (bytes < 0) 
    {
        if (errno != EAGAIN && errno != EWOULDBLOCK) 
        {
            perror("send");
            client->closeConnection();
        }
        return;
    }
    buffer.erase(0, bytes);
    if (buffer.empty())
    {
        epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.fd = client->getSocketFD();
        epoll_ctl(epollFD, EPOLL_CTL_MOD, client->getSocketFD(), &ev);
    }
}

void Server::handleClientRead(Listener* listener, Client* client)
{
    char buf[4096];
    int bytes = recv(client->getSocketFD(), buf, sizeof(buf), 0);
    if (bytes < 0) 
    {
        if (errno != EAGAIN && errno != EWOULDBLOCK) 
        {
            perror("recv");
            client->closeConnection();
        }
        return;
    }
    if (bytes == 0) 
    {
        client->closeConnection();
        return;
    }
    client->appendToReadBuffer(std::string(buf, bytes));
    if (client->getReadBuffer().find("\r\n\r\n") != string::npos)
    {
        string response =
            "HTTP/1.1 200 OK\r\n"
            "Content-Length: 13\r\n"
            "Content-Type: text/plain\r\n"
            "\r\n"
            "Hello, World!";

        client->appendToWriteBuffer(response);

        epoll_event ev;
        ev.events = EPOLLIN | EPOLLOUT;
        ev.data.fd = client->getSocketFD();
        epoll_ctl(epollFD, EPOLL_CTL_MOD, client->getSocketFD(), &ev);
    }
}

void Server::runEventLoop()
{
    const int MAX_EVENTS = 1024;
    epoll_event readyEvents[MAX_EVENTS];
    while (true)
    {
        int ready = epoll_wait(epollFD, readyEvents, MAX_EVENTS, -1);
        if (ready < 0)
        {
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < ready; ++i)
        {
            int fd = readyEvents[i].data.fd;

            Listener* listener = NULL;
            map<int, Listener*>::iterator it = listeners.find(fd);
            if (it != listeners.end())
            {
                listener = it->second;
                handleNewConnection(listener);
                continue;
            }

            Client* client = NULL;
            Listener* parentListener = NULL;

            for (it = listeners.begin(); it != listeners.end(); ++it)
            {
                client = it->second->getClients()[fd];
                if (client)
                {
                    parentListener = it->second;
                    break;
                }
            }

            if (!client)
                continue;

            uint32_t events = readyEvents[i].events;
            if (events & (EPOLLHUP | EPOLLERR))
            {
                epoll_ctl(epollFD, EPOLL_CTL_DEL, fd, NULL);
                if (parentListener)
                {
                    parentListener->removeClient(fd);
                }
                continue;
            }

            if (events & EPOLLIN)
            {
                handleClientRead(parentListener, client);
                cout << client->getReadBuffer() << endl;
            }
            if (events & EPOLLOUT)
                handleClientWrite(parentListener, client);
        }
    }
}

void Server::shutdown()
{
    for (map<int, Listener*>::iterator it = listeners.begin(); it != listeners.end(); ++it)
    {
        Listener* listener = it->second;
        listener->closeListener();
        delete listener;
    }
    listeners.clear();

    if (epollFD >= 0)
    {
        ::close(epollFD);
        epollFD = -1;
    }
}



std::vector<ServerConfig> GETconfig();

int main()
{
    std::vector<ServerConfig> serverconfig = GETconfig();
    Server server;
    server.loadListeners(serverconfig);
    server.initEventSystem();
    server.runEventLoop();
}





Client
{
    socketFD;          // File descriptor of the client socket

    readBuffer;     // Buffer storing data received from the client
                           // Example: HTTP request data
    writeBuffer;    // Buffer storing data waiting to be sent
                           // Example: HTTP response
};
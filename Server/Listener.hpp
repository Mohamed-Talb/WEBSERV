#ifndef LISTENER_HPP
#define LISTENER_HPP


#include <cctype>
#include <fstream>
#include <cerrno>
#include <cstring>
#include "../Helpers.hpp"
#include <cstring>
#include <stdexcept>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include "../configParser/configParser.hpp"

class Server;

class Listener : public IEventHandler 
{
        private:
        int socketFD;
        Server* server; 
        const std::vector<ServerConfig *> configs;
        
        Listener();
        Listener(const Listener &);

        public:
        virtual ~Listener();
        Listener(const std::vector<ServerConfig *> &confs, Server *srv);
        
        void handleEvent(int, uint32_t);

        int getFD() const;
        int getPort() const;
};


#endif
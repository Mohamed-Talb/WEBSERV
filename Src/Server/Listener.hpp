#ifndef LISTENER_HPP
#define LISTENER_HPP


#include <cctype>
#include <cerrno>
#include <fstream>
#include <cstring>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <stdexcept>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include "../Helpers.hpp"
#include "../configParser/configParser.hpp"

class Server;

class Listener : public IEventHandler 
{
        private:
        int socketFD;
        Server* server; 
        const std::vector<ServerConfig *> configs;

        public:
        virtual ~Listener();
        Listener(const std::vector<ServerConfig *> &confs, Server *srv);
        
        void handleEvent(int, uint32_t);

        int getFD() const;
        int getPort() const;
};


#endif
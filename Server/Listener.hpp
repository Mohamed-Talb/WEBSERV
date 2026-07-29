#ifndef LISTENER_HPP
#define LISTENER_HPP


#include <cctype>
#include <fstream>
#include "Client.hpp"
#include <cerrno>
#include <cstring>

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
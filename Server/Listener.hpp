#ifndef LISTENER_HPP
#define LISTENER_HPP

#include "Server.hpp"


class Listener : public IEventHandler 
{
        private:
        int socketFD;
        Server* server; 
        std::vector<ServerConfig> configs;
        
        Listener();
        Listener(const Listener &);

        public:
        virtual ~Listener();
        Listener(const std::vector<ServerConfig> &confs, Server *srv);
        
        virtual void handleRead();
        virtual void handleWrite();

        int getPort() const;
        virtual int  getFD() const;
};


#endif
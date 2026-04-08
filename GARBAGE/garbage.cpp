// #include <sys/socket.h>
// #include <netinet/in.h>
// #include <arpa/inet.h>
// #include <unistd.h>
// #include <netdb.h>
// #include <poll.h>
// #include <sys/epoll.h>
// // #include <sys/socket.h>
// #include <iostream>
// #include <vector>

// using namespace std;
// // int main()
// // {
// //     sockaddrin server;
// //     server.sinfamily = AFINET;
// //     server.sinport = htons(7070);
// //     server.sinaddr.saddr = INADDRANY;
    
// //     int serverFD = socket(AFINET, SOCKSTREAM, 0);
// //     bind(serverFD, (sockaddr*)&server, sizeof(server));
// //     listen(serverFD, 10);
// //     int clientFD = accept(serverFD, NULL, NULL);
// //     char buffer[1024] = {0};
// //     recv(clientFD, buffer, 1024, 0);
// //     std::cout  << buffer << std::endl;
// //     close(clientFD);
// //     close(serverFD);
// // }

// // multiClients with sekect 
// // int main()
// // {
// //     int serverFd = socket(AFINET, SOCKSTREAM, 0);
// //     sockaddrin addr{};
// //     addr.sinfamily = AFINET;
// //     addr.sinport = htons(8080);
// //     addr.sinaddr.saddr =INADDRANY;

// //     bind(serverFd, (sockaddr*)&addr, sizeof(addr));
// //     listen(serverFd,10);

// //     std::vector<pollfd> fds;
// //     pollfd serverPoll;
// //     serverPoll.fd = serverFd;
// //     serverPoll.events = POLLIN;
// //     fds.pushback(serverPoll);
// //     while (true)
// //     {
// //         int ready = poll(fds.data(), fds.size(), -1);
// //         if (ready < 0)
// //         {
// //             perror("poll");
// //             break ;
// //         }
// //         for (sizet i = 0; i < fds.size(); i++)
// //         {
// //             if(fds[i].revents & POLLIN)
// //             {
// //                 if (fds[i].fd == serverFd)
// //                 {
// //                     int clientFd = accept(serverFd, NULL, NULL);
// //                     pollfd clientPoll;
// //                     clientPoll.fd = clientFd;
// //                     fds.pushback(clientPoll);
// //                     cout << "NEW CLIENT CONNECTED";
// //                 }
// //                 else 
// //                 {
// //                     char buffer[1024];
// //                     int bytes = read(fds[i].fd, buffer, sizeof(buffer));
// //                     if (bytes <= 0)
// //                     {
// //                         close(fds[i].fd);
// //                         fds.erase(fds.begin() + 1);
// //                         i--;
// //                     }
// //                     else
// //                     {
// //                         buffer[bytes] = '\0';
// //                         std::cout << "MESSAGE: " << buffer << std::endl;
// //                     }
// //                 }
// //             }
// //         }
// //     }
// // }

// int main()
// {
//     int serverFd = socket(AFINET, SOCKSTREAM, 0);
//     sockadr addr{};
//     addr.sinfamily = AFINET;
//     addr.sinport = htons(8080);
//     addr.sinaddr.saddr =INADDRANY;

//     bind(serverFd, (sockaddr*)&addr, sizeof(addr));
//     listen(serverFd,10);

//     int epollFd = epollcreate(1);
//     epollevent ev;
//     epollevent events[10];

//     ev.events = EPOLLIN;
//     ev.data.fd = serverFd;
//     epollctl(epollFd, EPOLLCTLADD, serverFd, &ev);
//     while (true)
//     {
//         int ready = epollwait(epollFd, events, 10, -1);
//         for (int i = 0; i < ready; i++)
//         {
//             if (events[i].data.fd == serverFd)
//             {
//                 int clientFd = accept(serverFd, NULL, NULL);
//                 epollevent clientEvent;
//                 clientEvent.events = EPOLLIN;
//                 clientEvent.data.fd = clientFd;
//                 epollctl(epollFd, EPOLLCTLADD, clientFd, &clientEvent);
//             }
//             else 
//             {
//                 char buffer[1024];
//                 int bytes = read(events[i].data.fd, buffer, sizeof(buffer));
//                 if (bytes <= 0)
//                 {
//                     close(events[i].data.fd);
//                     epollctl(epollFd, EPOLLCTLDEL, events[i].data.fd, NULL);
//                 }
//                 else 
//                 {
//                     buffer[bytes] = '\0';
//                     std::cout << "MESSAGE: " << buffer << std::endl;
//                 }
//             }
//         }

//     }

// }

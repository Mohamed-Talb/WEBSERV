#include <iostream>
#include <string>

class Logger {
public:
    static void server(const std::string& msg) { std::cout << "\033[1;34m[SERVER]\033[0m : " << msg << std::endl; }
    static void listener(const std::string& msg) { std::cout << "\033[1;32m[LISTENER]\033[0m : " << msg << std::endl; }
    static void client(const std::string& msg) { std::cout << "\033[1;36m[CLIENT]\033[0m : " << msg << std::endl; }
    static void error(const std::string& msg) { std::cerr << "\033[1;31m[ERROR]\033[0m : " << msg << std::endl; }
};
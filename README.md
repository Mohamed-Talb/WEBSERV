# WEBSERV

Webserv is a non-blocking HTTP server written in C++98. It accepts client connections, parses HTTP requests, serves static resources, executes CGI scripts, and generates HTTP responses.

## MAIN FEATURES
1. Non-blocking sockets with Linux `epoll()`
2. Multiple servers, ports, and virtual hosts
3. `GET`, `POST`, and `DELETE` methods
4. Static files and directory indexes
5. File uploads and deletion
6. CGI execution using `fork()`, `execve()`, and pipes
7. Redirections and custom error pages
8. Persistent connections and timeouts
9. Configuration-based routing

## DOCUMENTATION
Detailed documentation is available in the `DOCS` directory:

- [Development Guide](Docs/DEV.md) - Architecture, main components, diagrams, and request flow.
- [User Guide](Docs/USER.md) - Installation, build instructions, tester setup, and website usage.
- [Configuration Guide](Docs/CONFIG.md) - Supported directives, syntax, rules, examples, and limitations.

## PROJECT STRUCTURE
```text
WEBSERV/
├── Configs/       Server configuration files
├── Docs/          Project documentation
├── Src/           C++ source code
├── Testers/       HTTP and CGI tester executables
├── Www/           Static files, CGI scripts, and test website
├── Makefile
└── README.md
```

## MAIN COMPONENTS
- **Server:** Accepts clients and manages file-descriptor events.
- **Configuration Parser:** Reads server blocks, locations, and routing rules.
- **HTTP:** Parses requests, resolves routes, and creates responses.
- **CGI:** Executes external scripts and returns dynamic content.

See the [User Guide](DOCS/USER.md) for the complete testing instructions.

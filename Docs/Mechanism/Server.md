# SERVER INITIALIZATION & I/O SETUP

## **ARCHITECTURE OVERVIEW**

The server follows an **EVENT-DRIVEN ARCHITECTURE** based on the **REACTOR PATTERN**. The **SERVER** acts as the central event manager using **EPOLL**. **LISTENERS** accept incoming connections, and **CLIENTS** represent active connections.

```abap
Server
 ├── Listener (PORT 8080)
 │     ├── Client
 │     ├── Client
 │     └── Client
 │
 └── Listener (PORT 8081)
       ├── Client
       └── Client
```

```abap
+--------------------+
|       Server       |
|--------------------|
| manages EPOLL      |
| manages listeners  |
| dispatches events  |
+----------+---------+
           |
           v
+--------------------+
|      Listener      |
|--------------------|
| owns socket        |
| accepts clients    |
| manages clients    |
+----------+---------+
           |
           v
+--------------------+
|       Client       |
|--------------------|
| socket connection  |
| read buffer        |
| write buffer       |
+--------------------+
```

- SERVER manages the EVENT SYSTEM using EPOLL and dispatches events.
- LISTENER manages a LISTENING SOCKET and accepts new connections.
- CLIENT represents a connected client and stores REQUEST and RESPONSE data.

# **CLIENT**

```abap
Client
{
    socketFD;      // SOCKET representing the connection with the CLIENT

    readBuffer;    // stores data received from the CLIENT
                   // example: HTTP REQUEST

    writeBuffer;   // stores data waiting to be sent
                   // example: HTTP RESPONSE
}
```

### **MAIN FUNCTIONS**

| FUNCTION | DESCRIPTION |
| --- | --- |
| closeConnection() | CLOSES the CLIENT SOCKET when the connection ends or an ERROR occurs |
| appendToReadBuffer(data) | APPENDS received DATA to the READ BUFFER after RECV |
| appendToWriteBuffer(data) | STORES RESPONSE DATA that will be sent later |
| hasPendingWrite() | CHECKS if the WRITE BUFFER still contains DATA |

# **LISTENER**

```abap
Listener
{
    socketFD;      // LISTENING SOCKET used to ACCEPT CONNECTIONS

    config;        // SERVER CONFIGURATION
                   // contains PORT, HOST, ROUTES

    clients;       // COLLECTION of connected CLIENTS
                   // maps CLIENT SOCKET -> CLIENT OBJECT
}
```

### **MAIN FUNCTIONS**

| FUNCTION | DESCRIPTION |
| --- | --- |
| loadListener(config) | CREATES the LISTENING SOCKET and prepares it using SOCKET → BIND → LISTEN |
| acceptClient() | ACCEPTS a NEW CLIENT CONNECTION and creates a CLIENT OBJECT |
| removeClient(fd) | REMOVES a CLIENT and CLOSES the CONNECTION |
| closeListener() | CLOSES the LISTENER SOCKET and ALL CLIENTS |

# **SERVER**

```abap
Server
{
    listeners;     // COLLECTION of LISTENERS
                   // maps SOCKET FD -> LISTENER

    epollFD;       // EPOLL INSTANCE monitoring all SOCKETS
}
```

### **MAIN FUNCTIONS**

| FUNCTION | DESCRIPTION |
| --- | --- |
| loadListeners(configs) | CREATES LISTENERS from SERVER CONFIGURATION |
| initEventSystem() | INITIALIZES the EPOLL SYSTEM and REGISTERS LISTENER SOCKETS |
| handleNewConnection(listener) | ACCEPTS a NEW CLIENT and REGISTERS it in EPOLL |
| handleClientRead(listener, client) | HANDLES EPOLLIN EVENTS and READS CLIENT DATA |
| handleClientWrite(listener, client) | HANDLES EPOLLOUT EVENTS and SENDS RESPONSE DATA |
| runEventLoop() | MAIN EVENT LOOP using EPOLL_WAIT |

# **SERVER WORKFLOW**

## **SERVER STARTUP**

```
main()
   |
   v
load CONFIGURATION
   |
   v
create LISTENERS
   |
   v
initialize EPOLL
   |
   v
run EVENT LOOP
```

## **CLIENT CONNECTION**

```
CLIENT connects
      |
      v
LISTENER SOCKET receives EVENT
      |
      v
handleNewConnection()
      |
      v
CLIENT registered in EPOLL
```

## **REQUEST HANDLING**

```
CLIENT sends REQUEST
      |
      v
EPOLLIN EVENT
      |
      v
handleClientRead()
      |
      v
REQUEST stored in READBUFFER
```

## **RESPONSE SENDING**

```
RESPONSE prepared
      |
      v
EPOLLOUT enabled
      |
      v
handleClientWrite()
      |
      v
RESPONSE sent to CLIENT
```


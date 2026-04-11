**CLIENT CLASS MANUAL**

The Client class acts as a wrapper around an individual CLIENT SOCKET. It handles the CONNECTION state and maintains the READ BUFFER and WRITE BUFFER for asynchronous data transfer.

| **FUNCTION** | **DESCRIPTION** | **RETURN VALUE** |
| :--- | :--- | :--- |
| `closeConnection()` | Safely closes the SOCKET FD if it is currently open. | `void` |
| `isConnected()` | Checks if the CLIENT has an active, valid SOCKET FD. | `bool` |
| `appendToReadBuffer(const std::string& data)` | Appends incoming data from the network into the internal READ BUFFER. | `void` |
| `getReadBuffer()` | Retrieves the current contents of the READ BUFFER. | `const std::string&` |
| `clearReadBuffer()` | Empties the READ BUFFER after a complete HTTP REQUEST is processed. | `void` |
| `appendToWriteBuffer(const std::string& data)` | Appends outbound HTTP RESPONSE data into the internal WRITE BUFFER. | `void` |
| `getWriteBuffer()` | Retrieves the current contents of the WRITE BUFFER. | `const std::string&` |
| `consumeWriteBuffer(size_t bytes)` | Removes a specific number of bytes from the front of the WRITE BUFFER after they are successfully sent. | `void` |
| `hasPendingWrite()` | Checks if there is unsent data remaining in the WRITE BUFFER. | `bool` |
| `getSocketFD()` | Returns the underlying SOCKET FD for this specific CLIENT. | `int` |


**LISTENER CLASS MANUAL**

The Listener class manages a server PORT. It handles the BIND and LISTEN operations, accepts new incoming CLIENT CONNECTIONS, and performs the actual network input/output operations.

| **FUNCTION** | **DESCRIPTION** | **RETURN VALUE** |
| :--- | :--- | :--- |
| `loadListener(const ServerConfig& conf)` | Creates the SOCKET, sets reuse options, performs the BIND, and puts the SOCKET into LISTEN mode. | `void` |
| `closeListener()` | Iterates through all connected CLIENT objects, closes their connections, and deletes them. | `void` |
| `acceptClient()` | Accepts a new CONNECTION, sets the new SOCKET FD to NON-BLOCKING mode, and creates a new Client object. | `Client*` |
| `getClient(int fd)` | Retrieves a specific Client pointer from the internal map using its SOCKET FD. | `Client*` |
| `removeClient(int clientFD)` | Closes, deletes, and removes a specific CLIENT from the internal management map. | `void` |
| `getSocketFD()` | Returns the main listening SOCKET FD for this PORT. | `int` |
| `getPort()` | Returns the PORT number this listener is configured to operate on. | `int` |
| `handleClientRead(Client* client)` | Receives data from the CLIENT into its READ BUFFER. Returns the IO STATE (e.g., IO_PENDING, IO_READY, IO_DISCONNECTED). | `IOState` |
| `handleClientWrite(Client* client)` | Sends data from the CLIENT WRITE BUFFER out to the network. | `IOState` |


**SERVER CLASS MANUAL**

The Server class is the core orchestrator. It sets up the EPOLL instance, groups configurations, and runs the main EVENT LOOP to dispatch network EVENTS.

| **FUNCTION** | **DESCRIPTION** | **RETURN VALUE** |
| :--- | :--- | :--- |
| `registerToEpoll(int fd, uint32_t events)` | Adds a specific SOCKET FD to the EPOLL instance to watch for specific EVENTS. | `void` |
| `init(std::vector<ServerConfig>& configs)` | Creates the EPOLL instance, groups configurations by PORT, and instantiates Listener objects. | `void` |
| `shutdown()` | Deletes all Listener objects and cleanly closes the main EPOLL FD. | `void` |
| `disconnectClient(Listener* listener, int clientFD)` | Removes a CLIENT from the EPOLL tracker and instructs the associated Listener to delete it. | `void` |
| `handleNewConnection(Listener* listener)` | Accepts a newly connected CLIENT and registers its SOCKET FD to EPOLL to monitor for EPOLLIN. | `void` |
| `handleConnection(Listener* listener, int clientFd, uint32_t event)` | Evaluates EPOLLIN or EPOLLOUT EVENTS and routes them to the appropriate Listener READ/WRITE handlers. Modifies EPOLL states based on buffers. | `void` |
| `runEventLoop()` | The infinite loop that executes EPOLL_WAIT and dispatches incoming EVENTS to either create new CONNECTIONS or handle data transfers. | `void` |
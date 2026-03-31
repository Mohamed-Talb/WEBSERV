# SERVER INITIALIZATION & I/O SETUP

This module sets up SERVER SOCKETS, binds to the configured INTERFACES and PORTS, and initializes the **NON-BLOCKING I/O** system using `poll()`, `select()`, or equivalent to handle multiple clients concurrently.
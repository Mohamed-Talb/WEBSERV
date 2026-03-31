# CGI SUPPORT

This module executes CGI SCRIPTS to provide dynamic content. It uses system calls like `fork()` and `execve()` to run the script and captures its output to return as the HTTP RESPONSE.
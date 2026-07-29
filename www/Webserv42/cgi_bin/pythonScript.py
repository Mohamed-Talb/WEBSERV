#!/usr/bin/env python3

# Print the required CGI headers with the blank line (\n\n) at the end
print("Status: 200 OK")
print("Content-Type: text/html\n")

# Print the actual body
print("<html>")
print("<head><title>CGI Success!</title></head>")
print("<body>")
print("<h1>Hello from Python!</h1>")
print("<p>If you can read this, your C++ webserv architecture is 100% working.</p>")
print("</body>")
print("</html>")

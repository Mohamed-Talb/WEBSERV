#!/usr/bin/env python3
import sys
import os

# Read the body from stdin (for POST requests)
content_length = int(os.environ.get('CONTENT_LENGTH', 0))
body = sys.stdin.read(content_length)

# Output headers
print("Content-type: text/html\r\n\r\n")

# Output HTML
print("<html><body style='background:#0d1117; color:white;'>")
print("<h1>CGI Success!</h1>")
print(f"<p>Raw POST Data received: {body}</p>")
print("<a href='/cgi.html'>Go Back</a>")
print("</body></html>")
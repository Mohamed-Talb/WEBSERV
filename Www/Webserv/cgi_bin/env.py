#!/usr/bin/env python3

import os

variables = [
    "REQUEST_METHOD",
    "SCRIPT_NAME",
    "PATH_INFO",
    "QUERY_STRING",
    "CONTENT_TYPE",
    "CONTENT_LENGTH",
    "SERVER_NAME",
    "SERVER_PORT",
    "SERVER_PROTOCOL",
    "HTTP_HOST",
    "HTTP_COOKIE",
    "HTTP_USER_AGENT"
]

body = ""

for name in variables:
    body += "{}={}\n".format(name, os.environ.get(name, ""))

print("Content-Type: text/plain; charset=utf-8")
print("Content-Length: {}".format(len(body.encode("utf-8"))))
print()
print(body, end="")
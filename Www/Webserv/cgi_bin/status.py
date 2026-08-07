#!/usr/bin/env python3

import os
from urllib.parse import parse_qs

parameters = parse_qs(os.environ.get("QUERY_STRING", ""))
code = parameters.get("code", ["201"])[0]

statuses = {
    "200": "OK",
    "201": "Created",
    "400": "Bad Request",
    "403": "Forbidden",
    "404": "Not Found",
    "500": "Internal Server Error"
}

reason = statuses.get(code, "OK")
body = "CGI STATUS: {} {}\n".format(code, reason)

print("Status: {} {}".format(code, reason))
print("Content-Type: text/plain; charset=utf-8")
print("Content-Length: {}".format(len(body.encode("utf-8"))))
print()
print(body, end="")
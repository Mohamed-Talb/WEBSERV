#!/usr/bin/env python3

body = "REDIRECTING TO HOME\n"

print("Status: 302 Found")
print("Location: /index.html")
print("Content-Type: text/plain; charset=utf-8")
print("Content-Length: {}".format(len(body.encode("utf-8"))))
print()
print(body, end="")
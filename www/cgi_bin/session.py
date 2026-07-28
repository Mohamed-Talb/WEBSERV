#!/usr/bin/env python3
"""Small CGI probe for webserv's server-owned session lifecycle."""

import html
import os
from urllib.parse import parse_qs

query = parse_qs(os.environ.get("QUERY_STRING", ""))
action = query.get("action", ["read"])[0]
session_id = os.environ.get("SESSION_ID", "")
cookie = os.environ.get("HTTP_COOKIE", "")

print("Content-Type: text/plain")
print()
print("action: " + html.escape(action))
print("session_id: " + html.escape(session_id))
print("cookie_has_session_id: " + str("session_id=" in cookie).lower())
print("request_method: " + os.environ.get("REQUEST_METHOD", ""))

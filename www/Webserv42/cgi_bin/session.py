#!/usr/bin/env python3

import os
import uuid
from urllib.parse import parse_qs


def get_cookie(name):
    cookie_header = os.environ.get("HTTP_COOKIE", "")

    for item in cookie_header.split(";"):
        item = item.strip()

        if "=" not in item:
            continue

        cookie_name, cookie_value = item.split("=", 1)

        if cookie_name.strip() == name:
            return cookie_value.strip()

    return ""


def get_action():
    query = os.environ.get("QUERY_STRING", "")
    parameters = parse_qs(query)

    values = parameters.get("action", [])

    if not values:
        return "read"

    return values[0]


def print_response(body, set_cookie=""):
    body_bytes = body.encode("utf-8")

    print("Status: 200 OK")
    print("Content-Type: text/plain; charset=utf-8")
    print("Content-Length: {}".format(len(body_bytes)))
    print("Cache-Control: no-store")

    if set_cookie:
        print("Set-Cookie: {}".format(set_cookie))

    print()
    print(body, end="")


action = get_action()
session_id = get_cookie("session_id")

if action == "create":
    if not session_id:
        session_id = str(uuid.uuid4())

    cookie = (
        "session_id={}; Path=/; HttpOnly; SameSite=Lax"
        .format(session_id)
    )

    print_response(
        "SESSION CREATED\n"
        "SESSION_ID={}\n".format(session_id),
        cookie
    )

elif action == "destroy":
    cookie = (
        "session_id=; Path=/; HttpOnly; SameSite=Lax; "
        "Max-Age=0"
    )

    print_response(
        "SESSION DESTROYED\n",
        cookie
    )

elif action == "read":
    if not session_id:
        print_response(
            "NO ACTIVE SESSION\n"
        )
    else:
        print_response(
            "EXISTING SESSION\n"
            "SESSION_ID={}\n".format(session_id)
        )

else:
    print_response(
        "UNKNOWN ACTION\n"
        "SUPPORTED ACTIONS: create, read, destroy\n"
    )

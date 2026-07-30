#!/usr/bin/env python3

import fcntl
import json
import os
import time
import uuid
from urllib.parse import parse_qs


SESSION_TTL = 3600
SCRIPT_DIRECTORY = os.path.dirname(os.path.abspath(__file__))
SESSION_FILE = os.path.join(SCRIPT_DIRECTORY, "sessions.json")


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


def get_parameters():
    query = os.environ.get("QUERY_STRING", "")
    return parse_qs(query, keep_blank_values=True)


def get_action(parameters):
    values = parameters.get("action", [])

    if not values:
        return "read"

    return values[-1]


def load_sessions(file):
    file.seek(0)
    content = file.read()

    if not content.strip():
        return {}

    try:
        sessions = json.loads(content)

        if isinstance(sessions, dict):
            return sessions
    except (json.JSONDecodeError, ValueError):
        pass

    return {}


def save_sessions(file, sessions):
    file.seek(0)
    file.truncate()

    json.dump(sessions, file, indent=2, sort_keys=True)
    file.flush()
    os.fsync(file.fileno())


def remove_expired_sessions(sessions):
    current_time = int(time.time())
    expired_ids = []

    for session_id, session in sessions.items():
        expires_at = session.get("expires_at", 0)

        if not isinstance(expires_at, int) or expires_at <= current_time:
            expired_ids.append(session_id)

    for session_id in expired_ids:
        del sessions[session_id]


def create_session(sessions):
    session_id = str(uuid.uuid4())
    current_time = int(time.time())

    sessions[session_id] = {
        "created_at": current_time,
        "expires_at": current_time + SESSION_TTL,
        "variables": {
            "username": "guest",
            "counter": 0
        }
    }

    return session_id


def session_exists(sessions, session_id):
    return session_id and session_id in sessions


def update_session(sessions, session_id, parameters):
    session = sessions[session_id]
    variables = session.setdefault("variables", {})

    if "username" in parameters:
        variables["username"] = parameters["username"][-1]

    if "value" in parameters:
        variables["value"] = parameters["value"][-1]

    variables["counter"] = int(variables.get("counter", 0)) + 1
    session["expires_at"] = int(time.time()) + SESSION_TTL


def format_session(session_id, session):
    variables = session.get("variables", {})

    return (
        "SESSION_ID={}\n"
        "CREATED_AT={}\n"
        "EXPIRES_AT={}\n"
        "VARIABLES={}\n"
    ).format(
        session_id,
        session.get("created_at", ""),
        session.get("expires_at", ""),
        json.dumps(variables, sort_keys=True)
    )


def print_response(body, status="200 OK", content_type="text/plain; charset=utf-8",
                   set_cookie=""):
    body_bytes = body.encode("utf-8")

    print("Status: {}".format(status))
    print("Content-Type: {}".format(content_type))
    print("Content-Length: {}".format(len(body_bytes)))
    print("Cache-Control: no-store")

    if set_cookie:
        print("Set-Cookie: {}".format(set_cookie))

    print()
    print(body, end="")


parameters = get_parameters()
action = get_action(parameters)
cookie_session_id = get_cookie("session_id")

os.makedirs(SCRIPT_DIRECTORY, exist_ok=True)

with open(SESSION_FILE, "a+", encoding="utf-8") as session_file:
    fcntl.flock(session_file.fileno(), fcntl.LOCK_EX)

    sessions = load_sessions(session_file)
    remove_expired_sessions(sessions)

    response_body = ""
    response_cookie = ""
    response_type = "text/plain; charset=utf-8"

    if action == "create":
        session_id = cookie_session_id

        if not session_exists(sessions, session_id):
            session_id = create_session(sessions)
            message = "NEW SESSION CREATED\n"
        else:
            message = "EXISTING SESSION USED\n"

        update_session(sessions, session_id, parameters)

        response_cookie = (
            "session_id={}; Path=/; HttpOnly; SameSite=Lax; Max-Age={}"
        ).format(session_id, SESSION_TTL)

        response_body = message + format_session(
            session_id,
            sessions[session_id]
        )

    elif action == "read":
        if session_exists(sessions, cookie_session_id):
            sessions[cookie_session_id]["expires_at"] = (
                int(time.time()) + SESSION_TTL
            )

            response_body = (
                "EXISTING SESSION\n"
                + format_session(
                    cookie_session_id,
                    sessions[cookie_session_id]
                )
            )
        else:
            response_body = "NO ACTIVE SESSION\n"

    elif action == "update":
        if not session_exists(sessions, cookie_session_id):
            response_body = "NO ACTIVE SESSION\n"
        else:
            update_session(sessions, cookie_session_id, parameters)

            response_body = (
                "SESSION UPDATED\n"
                + format_session(
                    cookie_session_id,
                    sessions[cookie_session_id]
                )
            )

    elif action == "destroy":
        if session_exists(sessions, cookie_session_id):
            del sessions[cookie_session_id]

        response_cookie = (
            "session_id=; Path=/; HttpOnly; SameSite=Lax; Max-Age=0"
        )

        response_body = "SESSION DESTROYED\n"

    elif action == "list":
        response_type = "application/json; charset=utf-8"
        response_body = json.dumps(
            sessions,
            indent=2,
            sort_keys=True
        ) + "\n"

    else:
        response_body = (
            "UNKNOWN ACTION\n"
            "SUPPORTED ACTIONS: create, read, update, destroy, list\n"
        )

    save_sessions(session_file, sessions)
    fcntl.flock(session_file.fileno(), fcntl.LOCK_UN)

print_response(
    response_body,
    content_type=response_type,
    set_cookie=response_cookie
)
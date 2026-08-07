#!/usr/bin/env python3
import os
import sys

# Read POST data from stdin
content_length = int(os.environ.get('CONTENT_LENGTH', 0))
post_data = sys.stdin.read(content_length)

print("Content-Type: text/html")
print()
print(f"""
<html>
<body>
    <h1>POST Data Received</h1>
    <pre>Raw data: {post_data}</pre>
    <h2>Environment:</h2>
    <pre>REQUEST_METHOD: {os.environ.get('REQUEST_METHOD')}
CONTENT_TYPE: {os.environ.get('CONTENT_TYPE')}
CONTENT_LENGTH: {os.environ.get('CONTENT_LENGTH')}</pre>
</body>
</html>
""")
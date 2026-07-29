# handle_form.py
import os
import sys

# Read POST body
content_length = int(os.environ.get('CONTENT_LENGTH', 0))
post_data = sys.stdin.read(content_length)

print("Content-Type: text/html")
print()
print(f"<h1>POST Data: {post_data}</h1>")
import os

print("Content-type: text/html\n\n")
query = os.environ.get("QUERY_STRING")

vars = query.split('&')
for var in vars:
    print(f"Hello {var[var.find('=') + 1:]}!\n")

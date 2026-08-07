# WEBSERV CONFIGURATION GUIDE

This document explains the configuration syntax supported by Webserv.

## 1. BASIC STRUCTURE
A configuration file contains one or more `server` blocks:
```nginx
server {
    listen 0.0.0.0:8080;
    root ./Www/Webserv;
    server_name localhost;

    location / {
        methods GET;
        index index.html;
    }
}
```
Every directive must end with a semicolon. A `location` uses braces and must be placed inside a `server` block.

## 2. SERVER DIRECTIVES
The following directives are supported directly inside `server`.

### LISTEN
Defines the address and port:
```nginx
listen 0.0.0.0:8080;
listen 8080;
listen localhost:8080;
```
When only a port is provided, Webserv uses `0.0.0.0`. `localhost` is converted to `127.0.0.1`. The port must be between `1` and `65535`.
### ROOT
Defines the server's document root:
```nginx
root ./Www/Webserv;
```
### SERVER_NAME
Defines one or more names for virtual-host selection:
```nginx
server_name localhost webserv.local;
```
The request `Host` header is used to select the matching server configuration.
### INDEX
Defines one or more default directory files:
```nginx
index index.html index.htm;
```
### ERROR_PAGE
Maps one or more error codes to a page:
```nginx
error_page 404 /errors/404.html;
error_page 500 502 504 /errors/50x.html;
```
### CLIENT_MAX_BODY_SIZE
Defines the maximum accepted request-body size:
```nginx
client_max_body_size 100;
client_max_body_size 10KB;
client_max_body_size 5MB;
```
Supported units are `B`, `K`, `KB`, `M`, `MB`, `G`, and `GB`. Values must be positive integers; decimal values are not supported.
### LOCATION
Creates rules for a request path:
```nginx
location /upload {
    methods GET POST DELETE;
    root ./Www/Webserv;
}
```
Location paths must start with `/`.
## 3. LOCATION DIRECTIVES
The following directives are supported inside a `location` block.
### METHODS
Defines the allowed HTTP methods:
```nginx
methods GET POST DELETE;
```
Supported methods are `GET`, `POST`, and `DELETE`. Method names are converted to uppercase.
### ROOT
Overrides the server root for this location:
```nginx
root ./Www/YoupiBanane;
```
### INDEX
Overrides the server index list:
```nginx
index index.html youpi.bad_extension;
```
### AUTOINDEX
Enables or disables directory listing:
```nginx
autoindex on;
```
Only `on` and `off` are accepted.
### CLIENT_MAX_BODY_SIZE
Overrides the server body-size limit:
```nginx
client_max_body_size 100;
```
### CGI_PASS
Maps a file extension to a CGI executable or interpreter:
```nginx
cgi_pass .bla ./Testers/cgi_tester;
cgi_pass .py /usr/bin/python3;
```
The leading dot is optional, so `py` is stored as `.py`. An extension can contain letters, numbers, `_`, and `-`.
More than one CGI mapping can be added when each mapping uses a different extension.
### RETURN
Creates a redirection:
```nginx
return 301 /new-path;
return 302 https://example.com;
```
Only status codes `301` and `302` are supported. The target must start with `/`, `http://`, or `https://`.
### UPLOAD
Enables or disables uploads:
```nginx
upload on;
```
Only `on` and `off` are accepted.
### UPLOAD_PATH
Defines where uploaded files are stored:
```nginx
upload_path ./Www/Webserv/uploads;
```
When `upload on` is used, `upload_path` is required. An upload path is rejected when uploads are disabled.
## 4. INHERITANCE
A location inherits these values from its server when they are not set inside the location:
- `root`
- `index`
- `client_max_body_size`
Example:
```nginx
server {
    listen 8080;
    root ./Www/Webserv;
    index index.html;
    client_max_body_size 1MB;

    location / {
        methods GET;
    }
}
```
The `/` location uses the server's root, index, and body-size limit.
## 5. COMPLETE EXAMPLE
```nginx
server {
    listen 0.0.0.0:8080;
    server_name localhost;
    root ./Www/Webserv;
    index index.html;
    error_page 404 /errors/404.html;
    client_max_body_size 1MB;

    location / {
        methods GET;
        autoindex off;
    }

    location /upload {
        methods GET POST DELETE;
        upload on;
        upload_path ./Www/Webserv/uploads;
    }

    location /cgi_bin {
        methods GET POST;
        cgi_pass .py /usr/bin/python3;
    }

    location /old {
        methods GET;
        return 301 /;
    }
}
```

## 6. PATH RULES
Webserv normalizes configured paths by:
1. Removing repeated slashes
2. Removing unnecessary trailing slashes
3. Keeping `/` as the root location
4. Removing leading slashes from index names
5. Adding a leading slash to error-page paths
6 Adding a leading dot to CGI extensions

Paths containing `..` are rejected.

## 7. WHAT IS NOT SUPPORTED
Only the directives documented above are supported. The parser rejects:

1. Unknown directives
2. Nested `location` blocks
3. Duplicate server or location paths
4. Duplicate single-value directives
5. Duplicate methods, CGI extensions, or error codes
6. Missing semicolons or directive values
7. Invalid ports and body-size values
8. Location paths that do not start with `/`
9. Paths containing `..`
10. Redirect codes other than `301` and `302`
11. HTTP methods other than `GET`, `POST`, and `DELETE`
12. NGINX directives that are not listed in this guide
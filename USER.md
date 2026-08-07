# WEBSERV USER GUIDE

This guide explains how to build Webserv, run the tester, and use the included test website.

## 1. REQUIREMENTS
You need:

- A Linux environment
- A C++ compiler supporting C++98
- `make`
- A web browser or `curl`

On Ubuntu or Debian, install the required build tools with:

```bash
sudo apt update
sudo apt install build-essential
```

## 2. BUILD THE PROJECT
From the project root, run:
```bash
make
```
This creates the `webserv` executable in the project root.
To rebuild the project completely, run:
```bash
make re
```

## 3. RUN WEBSERV
Start Webserv by passing it a configuration file:
```bash
./webserv path/to/config.conf
```
For example, start it with the tester configuration:
```bash
./webserv Configs/tester.conf
```
Stop the server with `Ctrl+C`.

## 4. TESTER SETUP
The tester files are already included in the `Testers` directory:
```text
Testers/
├── tester
└── cgi_tester
```
- `tester` sends HTTP tests to Webserv.
- `cgi_tester` is executed by Webserv during CGI tests.
Make both files executable:
```bash
chmod +x Testers/tester Testers/cgi_tester
```

### REQUIRED TESTER FILES
The required `YoupiBanane` directory and its test files are already included in:
```text
Www/YoupiBanane
```
No additional directory or file setup is required.

### TESTER CONFIGURATION
The tester configuration is located at:
```text
Configs/tester.conf
```
It provides the required rules:
1. `/` accepts `GET` only.
2. Files ending in `.bla` use `Testers/cgi_tester` for `POST` requests.
3. `/post_body` accepts `POST` with a maximum body size of 100 bytes.
4. `/directory/` accepts `GET` and uses `Www/YoupiBanane` as its root.
5. When no file is requested from `/directory/`, the server searches for `youpi.bad_extension`.

### RUN THE TESTER
Open the first terminal from the project root and start Webserv:
```bash
./webserv Configs/tester.conf
```
Open a second terminal from the project root and run:
```bash
./Testers/tester http://localhost:8080
```
Follow the tester instructions and press Enter when requested.
Passing this tester is the minimum requirement. It should not be the only testing performed on the project.

## 5. TEST WEBSITE
The project includes a browser-based test website under:
```text
Www/Webserv
```
Its configuration is located at:
```text
Configs/webserv.conf
```
Start Webserv with this configuration:
```bash
./webserv Configs/webserv.conf
```
Then open this address in your browser:
```text
http://localhost:8080
```
The website provides the following pages:

1. **About:** General information about Webserv.
2. **Basic Tests:** Test basic `GET` and `POST` requests.
3. **Upload:** Upload files using `multipart/form-data`.
4. **Uploads:** View, download, and delete uploaded files.
5. **CGI Test:** Send `GET` and `POST` requests to CGI scripts.
6. **Session:** Create, read, and destroy cookie-based sessions.
7. **Virtual Hosts:** Test server selection using the `Host` header and listening port.

## 6. QUICK TESTS WITH CURL
Start Webserv before running these commands.
Test the home page:
```bash
curl -v http://localhost:8080/
```
Test a POST request:
```bash
curl -v -X POST -d "name=test" http://localhost:8080/post_body
```
Test a directory resource:
```bash
curl -v http://localhost:8080/directory/
```

## 7. COMMON PROBLEMS
### PERMISSION DENIED
Make the executables runnable:
```bash
chmod +x webserv Testers/tester Testers/cgi_tester
```

### ADDRESS ALREADY IN USE
Stop the process using the configured port or change the port in the configuration file.

### CGI DOES NOT RUN
Check that:
1. `Testers/cgi_tester` exists.
2. It has execution permission.
3. Its path matches the CGI path in `Configs/tester.conf`.
4. Webserv was started from the project root.

### WEBSITE DOES NOT OPEN
Check the host and port inside `Configs/webserv.conf`, then use the same address in the browser.

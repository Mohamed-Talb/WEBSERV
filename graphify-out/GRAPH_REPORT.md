# Graph Report - WEBSERV  (2026-09-22)

## Corpus Check
- Large corpus: 109 files · ~1,020,865 words. Semantic extraction will be expensive (many Claude tokens). Consider running on a subfolder.

## Summary
- 842 nodes · 1555 edges · 46 communities (39 shown, 7 thin omitted)
- Extraction: 85% EXTRACTED · 15% INFERRED · 0% AMBIGUOUS · INFERRED: 230 edges (avg confidence: 0.85)
- Token cost: 0 input · 0 output

## Community Hubs (Navigation)
- Server Runtime and Clients
- HTTP Header Parsing
- Client CGI Integration
- Request Body Parsing
- CGI Execution Internals
- CGI Workflow Diagram
- Architecture Documentation
- Server Lifecycle Diagram
- Location Configuration
- Web Test Pages
- Graphify Tooling
- Epoll Handler Registry
- Session CGI Script
- Configuration Dispatch
- HTTP Method Responses
- Server Event Loop
- Server Configuration
- Request Parsing Diagram
- Configuration Tokenizer
- Client State Machine
- HTTP Response Building
- Class Relationships Diagram
- Configuration Validation
- Request Routing
- Request Target Parsing
- Server Directive Validation
- Filesystem Utilities
- HTTP Result Variant
- Config Parser Orchestration
- Multipart Upload Handling
- Session Frontend
- Uploads Frontend
- Autoindex and Error Pages
- Sample Computer Image
- Route Match Data
- Server Error Pages
- Listen Endpoint
- CGI Test Frontend
- Theme Frontend
- Upload Test Frontend
- Basic Tests Frontend
- Virtual Host Tests
- Shell Test Suite

## God Nodes (most connected - your core abstractions)
1. `HttpRequest` - 59 edges
2. `ServerConfig` - 46 edges
3. `Client` - 39 edges
4. `Location` - 38 edges
5. `ConfigParser` - 32 edges
6. `CGI` - 31 edges
7. `RequestParser` - 29 edges
8. `HttpResponse` - 24 edges
9. `throwConfigError()` - 22 edges
10. `Server` - 19 edges

## Surprising Connections (you probably didn't know these)
- `URL Decoding and Path Security Tests` --semantically_similar_to--> `Path Normalization and Validation`  [INFERRED] [semantically similar]
  Www/Webserv/basic-tests.html → Docs/CONFIG.md
- `Server Configuration HTTP and CGI` --semantically_similar_to--> `Webserv Architecture`  [INFERRED] [semantically similar]
  Www/Webserv/about.html → Docs/DEV.md
- `CGI Script Runner` --semantically_similar_to--> `CGI Workflow`  [INFERRED] [semantically similar]
  Www/Webserv/cgi-test.html → Docs/DEV.md
- `CGI Request Inputs` --conceptually_related_to--> `CGI and Upload Configuration`  [INFERRED]
  Www/Webserv/cgi-test.html → Docs/CONFIG.md
- `403 Forbidden Page` --conceptually_related_to--> `Path Normalization and Validation`  [INFERRED]
  Www/Webserv/errors/403.html → Docs/CONFIG.md

## Import Cycles
- None detected.

## Hyperedges (group relationships)
- **Graphify Build Lifecycle** — _codex_skills_graphify_skill_structural_and_semantic_extraction, _codex_skills_graphify_skill_persistent_knowledge_graph, _codex_skills_graphify_skill_community_analysis_and_exports [EXTRACTED 1.00]
- **Webserv Request Lifecycle** — docs_dev_epoll_event_loop, docs_dev_client_state_machine, docs_dev_http_request_parser, docs_dev_http_routing_and_handler, docs_dev_cgi_workflow, docs_dev_complete_request_flow [EXTRACTED 1.00]
- **Webserv Browser Validation Suite** — docs_user_browser_test_website, www_webserv_basic_tests_http_behaviour_checks, www_webserv_cgi_test_cgi_script_runner [INFERRED 0.95]

## Communities (46 total, 7 thin omitted)

### Community 0 - "Server Runtime and Clients"
Cohesion: 0.07
Nodes (49): algorithm, cctype, cerrno, csignal, cstddef, cstdio, cstdlib, cstring (+41 more)

### Community 1 - "HTTP Header Parsing"
Cohesion: 0.06
Nodes (64): toLower(), trim(), StepStatus, string, vector, hasSingleValue(), isValidHeaderName(), isValidHeaderValue() (+56 more)

### Community 2 - "Client CGI Integration"
Cohesion: 0.08
Nodes (42): toString, CGI, Client, activeCgi, activeConfig, appendToReadBuffer, appendToWriteBuffer, Client::Client() (+34 more)

### Community 3 - "Request Body Parsing"
Cohesion: 0.07
Nodes (38): ParseStatus, StepStatus, string, RequestParser::bodyParser(), RequestParser::parseChunkedBody(), RequestParser::parseNormalBody(), string, vector (+30 more)

### Community 4 - "CGI Execution Internals"
Cohesion: 0.11
Nodes (32): CgiState, Client, pid_t, CGI, buildEnv, CGI::CGI(), cgiPid, closeInput (+24 more)

### Community 5 - "CGI Workflow Diagram"
Cohesion: 0.08
Nodes (30): Append to rawOutputBuffer, Both Pipes Closed?, Build CGI Environment, CGI Constructor, HttpRequest and CGI Route Information, CGI Workflow Diagram, Child Closes Unused Pipe Ends, Client::onCgiDone() (+22 more)

### Community 6 - "Architecture Documentation"
Cohesion: 0.09
Nodes (30): CGI and Upload Configuration, Configuration Inheritance, Webserv Configuration Language, Location Blocks, Path Normalization and Validation, Server Blocks, Virtual Host Selection, CGI Workflow (+22 more)

### Community 7 - "Server Lifecycle Diagram"
Cohesion: 0.10
Nodes (25): Accept New Client, Client Can Be Read, Close Client, Complete Request, Create HttpResponse, Main Flow Diagram, Event-Driven HTTP Server Lifecycle, Execute CGI Script and Read Its Output (+17 more)

### Community 8 - "Location Configuration"
Cohesion: 0.13
Nodes (23): checkDuplicate, Location, allowedMethods, autoindex, cgiMappings, client_max_body_size, indexes, methods (+15 more)

### Community 9 - "Web Test Pages"
Cohesion: 0.10
Nodes (25): ALPHA VIRTUAL HOST, BETA VIRTUAL HOST, DEFAULT SERVER, SERVER ON PORT 9090, CGI request testing, Static file testing, Webserv HTTP/1.1 test suite, HttpOnly session identifier cookie (+17 more)

### Community 10 - "Graphify Tooling"
Cohesion: 0.11
Nodes (24): Folder Watcher, URL Ingestion, Graph Exports, MCP Graph Server, Confidence Audit Trail, Semantic Extraction Contract, Cross-Repository Graph, Monorepo Merge Flow (+16 more)

### Community 11 - "Epoll Handler Registry"
Cohesion: 0.12
Nodes (24): accept() clients, Add handler to deletionQueue, CGI::handleInput(), CGI::handleOutput(), CGI input FD + EPOLLOUT, CGI output FD + EPOLLIN, Client FD + EPOLLIN, Client FD + EPOLLOUT (+16 more)

### Community 12 - "Session CGI Script"
Cohesion: 0.09
Nodes (7): fcntl, json, os, sys, time, urllib_parse, uuid

### Community 13 - "Configuration Dispatch"
Cohesion: 0.09
Nodes (23): LocationHandler, ServerHandler, ConfigParser, locationAutoindex, locationCgiMapping, locationClientMaxBodySize, locationDispatch, locationIndex (+15 more)

### Community 14 - "HTTP Method Responses"
Cohesion: 0.13
Nodes (22): 404 Not Found, 405 Method Not Allowed, 501 Not Implemented, File exists or POST?, Generate autoindex response, HTTP Request Routing Workflow, HttpHandler::process(), HttpMethods::DELETE() (+14 more)

### Community 15 - "Server Event Loop"
Cohesion: 0.15
Nodes (21): set, IEventHandler, vector, IEventHandler, map, vector, Server, addHandler (+13 more)

### Community 16 - "Server Configuration"
Cohesion: 0.11
Nodes (19): validateLocation, map, vector, ServerConfig, client_max_body_size, errorPage, host, indexes (+11 more)

### Community 17 - "Request Parsing Diagram"
Cohesion: 0.13
Nodes (20): Body result, Client readBuffer, Configuration found?, Content-Length exceeds limit?, Error 413, Error 500, Headers result, HTTP Request Parsing Flowchart (+12 more)

### Community 18 - "Configuration Tokenizer"
Cohesion: 0.23
Nodes (17): string, Token, column, line, text, string, vector, TokenStream (+9 more)

### Community 19 - "Client State Machine"
Cohesion: 0.17
Nodes (18): CGI Calls onCgiDone(), CGI Failure Closes Client, CGI Route Selected, CGI Timeout Creates 504, Client Created, Client Lifecycle State Machine, Client State Machine Diagram, CLOSED (+10 more)

### Community 20 - "HTTP Response Building"
Cohesion: 0.21
Nodes (16): parseCgiOutput, string, map, string, vector, HttpResponse, addHeader, body (+8 more)

### Community 21 - "Class Relationships Diagram"
Cohesion: 0.18
Nodes (17): CGI, CGI Script, Client, ConfigParser, Horizontal Cylinder, HTTP Client, HttpHandler, HttpRequest (+9 more)

### Community 22 - "Configuration Validation"
Cohesion: 0.28
Nodes (16): string, TokenStream, getErrorMessage(), throwConfigError(), ConfigParser::locationCgiMapping(), string, TokenStream, vector (+8 more)

### Community 23 - "Request Routing"
Cohesion: 0.17
Nodes (16): string, vector, HttpHandler, isCgiRequest, isMethodAllowed, process, resolveIndexFiles, resolveRedirection (+8 more)

### Community 24 - "Request Target Parsing"
Cohesion: 0.20
Nodes (13): hexDigit(), StepStatus, string, parseRequestLineValues(), RequestParser::requestLineParser(), splitTarget(), validateRequestTarget(), string (+5 more)

### Community 25 - "Server Directive Validation"
Cohesion: 0.22
Nodes (13): ConfigParser::serverErrorPages(), ConfigParser::serverListen(), string, intToString(), isOnlyDigits(), isValidErrorCode(), isValidHost(), isValidServerName() (+5 more)

### Community 26 - "Filesystem Utilities"
Cohesion: 0.28
Nodes (11): string, deleteFile(), fileExists(), getAbsolutePath(), hasAccessDenied(), isReadable(), isReadableFile(), isRegularFile() (+3 more)

### Community 27 - "HTTP Result Variant"
Cohesion: 0.21
Nodes (9): HttpResultType, string, resolveCgi, HttpResult, cgiInterpreter, cgiLocation, cgiRequestPath, response (+1 more)

### Community 28 - "Config Parser Orchestration"
Cohesion: 0.23
Nodes (10): TokenStream, CompareLocations, ConfigParser::ConfigParser(), initLocationDispatch, initServerDispatch, loadeConfig, parseServerBlock, validateServer (+2 more)

### Community 29 - "Multipart Upload Handling"
Cohesion: 0.30
Nodes (10): string, extractFilename(), HttpMethods::POST(), isSafeFilename(), MultipartFileInfo, contentLength, contentStart, filename (+2 more)

### Community 30 - "Session Frontend"
Cohesion: 0.21
Nodes (11): clearLogBtn, createBtn, destroyBtn, getSessionPath(), getSessionValue(), getUsername(), listBtn, log (+3 more)

### Community 31 - "Uploads Frontend"
Cohesion: 0.25
Nodes (6): clearBtn, listEl, log, refreshBtn, renderEmpty(), renderFromNames()

### Community 32 - "Autoindex and Error Pages"
Cohesion: 0.43
Nodes (7): isDirectory(), joinPath(), resolveDirectory, resolveAutoIndexing(), string, ErrorPage(), getReasonPhrase()

### Community 33 - "Sample Computer Image"
Cohesion: 0.39
Nodes (8): Connection cables, CRT monitor, Desktop computer tower, Computer keyboard, Sheet of paper on computer tower, Personal computing, Vintage desktop computer setup, Wired pointing device

### Community 34 - "Route Match Data"
Cohesion: 0.33
Nodes (6): string, RouteMatch, fullPath, location, requestPath, root

### Community 35 - "Server Error Pages"
Cohesion: 0.33
Nodes (6): 500 Internal Server Error page, Unexpected server condition, 501 Not Implemented page, Unsupported requested functionality, CGI process timeout, 504 Gateway Timeout page

### Community 36 - "Listen Endpoint"
Cohesion: 0.40
Nodes (4): string, Listen, host, port

### Community 38 - "CGI Test Frontend"
Cohesion: 0.50
Nodes (3): clearBtn, log, runBtn

### Community 40 - "Upload Test Frontend"
Cohesion: 0.50
Nodes (3): clearBtn, log, runBtn

## Knowledge Gaps
- **187 isolated node(s):** `pipeInFd`, `pipeOutFd`, `cgiPid`, `writeOffset`, `requestBody` (+182 more)
  These have ≤1 connection - possible missing edges or undocumented components. (Counts symbols only; 285 node(s) total have ≤1 connection when file, concept and rationale nodes are included.)
- **7 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `ServerConfig` connect `Server Configuration` to `Server Runtime and Clients`, `Autoindex and Error Pages`, `Client CGI Integration`, `Request Body Parsing`, `CGI Execution Internals`, `Listen Endpoint`, `Location Configuration`, `Server Event Loop`, `HTTP Response Building`, `Request Routing`, `Server Directive Validation`, `Filesystem Utilities`, `Config Parser Orchestration`, `Multipart Upload Handling`?**
  _High betweenness centrality (0.120) - this node is a cross-community bridge._
- **Why does `HttpRequest` connect `HTTP Header Parsing` to `Autoindex and Error Pages`, `Server Runtime and Clients`, `Client CGI Integration`, `Request Body Parsing`, `CGI Execution Internals`, `Request Routing`, `Request Target Parsing`, `Multipart Upload Handling`?**
  _High betweenness centrality (0.077) - this node is a cross-community bridge._
- **Why does `RequestParser` connect `Request Body Parsing` to `Server Runtime and Clients`, `Server Configuration`, `Client CGI Integration`, `HTTP Header Parsing`?**
  _High betweenness centrality (0.043) - this node is a cross-community bridge._
- **Are the 2 inferred relationships involving `HttpRequest` (e.g. with `onCgiDone` and `processReadBuffer`) actually correct?**
  _`HttpRequest` has 2 INFERRED edges - model-reasoned connections that need verification._
- **What connects `pipeInFd`, `pipeOutFd`, `cgiPid` to the rest of the system?**
  _187 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `Server Runtime and Clients` be split into smaller, more focused modules?**
  _Cohesion score 0.07192982456140351 - nodes in this community are weakly interconnected._
- **Should `HTTP Header Parsing` be split into smaller, more focused modules?**
  _Cohesion score 0.05583972719522592 - nodes in this community are weakly interconnected._
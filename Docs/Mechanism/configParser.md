# CONFIGURATION FILE PARSING

This module handles reading and parsing the **configuration file**. It extracts SERVER settings, ROUTES, ERROR PAGES, MAX BODY SIZE, and UPLOAD rules, storing them in structured data accessible to the rest of the server.

---

## 1. Internal Workflow

The parsing happens in three distinct phases:

1.  **Lexical Cleaning (`prepConf`):**
    * Removes comments (anything after `#`).
    * Adds "padding" spaces around structural delimiters (`{`, `}`, `;`).
    * Splits the string into a flat `std::vector<std::string>` of tokens using `std::stringstream`.

2.  **Recursive Parsing:**
    * **`parseTokens`**: The entry point. It identifies `server` blocks and initializes the process.
    * **`parseServer`**: Processes server-level directives (host, port, root). When it encounters a `location` keyword, it hands control to the sub-parser.
    * **`parseLocation`**: Handles route-specific rules. It processes tokens until it hits a closing `}`, then returns control back to the server level.

3.  **Data Storage:**
    * Results are stored in `ServerConfig` and `Location` structs.
    * Uses `std::map<int, std::string>` for error pages to allow for efficient $O(\log n)$ lookups during request handling.

---

## 2. Key Technical Implementations

### Iterator-Based State Management
To avoid expensive string copying and maintain global position, the parser passes a **Reference to a Vector Iterator** (`std::vector<std::string>::iterator &it`).
* Functions "consume" tokens by advancing the iterator.
* Because it is passed by reference, sub-functions update the caller's position automatically.

### Safety & Validation
* **Delimiter Padding:** Ensures that `server{` and `server {` are both treated as distinct tokens (`server` + `{`).
* **Boundary Checking:** Uses explicit `it != tokens.end()` checks to prevent Segmentation Faults on malformed files (e.g., missing semicolons).
* **C++98 Compliance:** Uses explicit iterator loops and `std::pair` insertions to ensure compatibility with strict compiler flags.

---

## 3. Data Structures

| Struct | Purpose | Key Members |
| :--- | :--- | :--- |
| **`ServerConfig`** | Top-level server settings | `host`, `port`, `serverName`, `Locations`, `errorPage` |
| **`Location`** | Route-specific behavior | `path`, `methods`, `root`, `index`, `cgiPath` |

> **Note for the Team:** When adding new directives, add the field to the corresponding struct and insert a new `else if (*it == "your_keyword")` branch in the parser. The `prepConf` logic handles the tokenization automatically.
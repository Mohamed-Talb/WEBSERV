#include "configParser.hpp"

/*
===============================================================================
 PATH HANDLING RULES (CONFIG PARSER + HTTP RESOLUTION)
===============================================================================

We follow Postel’s Law:
→ Accept flexible input
→ Normalize internally for consistency and safety

------------------------------------------------------------------------------
| ELEMENT          | ACCEPTED INPUT              | STORED FORMAT              |
------------------------------------------------------------------------------
| root             | "./www", "./www/"           | "./www"                    |
| location.path    | "/", "/img", "/img/"        | "/", "/img"                |
| index            | "index.html", "/index.html" | "index.html"               |
| cgi_path         | "./cgi-bin", "./cgi-bin/"   | "./cgi-bin"                |
| cgi_ext          | ".py", "py"                 | ".py"                      |
| error_page path  | "404.html", "/404.html"     | "/404.html"                |
------------------------------------------------------------------------------

------------------------------------------------------------------------------
 NORMALIZATION RULES
------------------------------------------------------------------------------

1. ROOT
   - Remove trailing slashes
   - Example:
        "./www/" → "./www"

2. LOCATION PATH
   - Must start with '/'
   - Remove trailing slash (except "/")
   - Example:
        "/images/" → "/images"

3. INDEX
   - Remove leading slashes
   - Must be relative to root
   - Reject ".." (security)
   - Example:
        "/index.html" → "index.html"

4. CGI EXT
   - Must start with '.'
   - Example:
        "py" → ".py"

5. ERROR PAGE PATH
   - Must start with '/'
   - Reject ".."
   - Example:
        "404.html" → "/404.html"

------------------------------------------------------------------------------
 SECURITY RULES
------------------------------------------------------------------------------

- Reject any path containing ".." (directory traversal)
- Never allow escaping the root directory

Multiple slashes should be collapsed:

    "///img//cat.png" → "/img/cat.png"
*/

int ConfigParser::parsePortValue(const std::string &value)
{
    if (!isOnlyDigits(value))
        throw std::runtime_error("Invalid port: " + value);

    if (value.size() > 5)
        throw std::runtime_error("Invalid port: " + value);

    int port = std::atoi(value.c_str());

    if (port <= 0 || port > 65535)
        throw std::runtime_error("Invalid port: " + value);

    return port;
}


size_t ConfigParser::parseBodySizeValue(const std::string &value)
{
    if (value.empty())
        throw std::runtime_error("Empty client_max_body_size directive value");

    size_t i = 0;
    while (i < value.size() && std::isdigit(static_cast<unsigned char>(value[i])))
    {
        i++;
    }
    if (i < value.size() && value[i] == '.')
    {
        throw std::runtime_error("Invalid client_max_body_size: decimals are not allowed.");
    }
    if (i == 0)
        throw std::runtime_error("Invalid body size syntax (missing digits): " + value);

    std::string numericPart = value.substr(0, i);
    std::string unitPart = value.substr(i);

    size_t baseSize = 0;
    std::istringstream iss(numericPart);
    iss >> baseSize;

    if (iss.fail())
        throw std::runtime_error("Numeric overflow parsing body size base: " + numericPart);
    std::string unit = toUpper(trim(unitPart)); 
    size_t multiplier = 1;

    if (unit.empty() || unit == "B") 
    {
        multiplier = 1;
    }
    else if (unit == "K" || unit == "KB")
    {
        multiplier = 1024;
    }
    else if (unit == "M" || unit == "MB")
    {
        multiplier = 1024 * 1024;
    } 
    else if (unit == "G" || unit == "GB")
    {
        multiplier = 1024 * 1024 * 1024;
    }
    else
    {
        throw std::runtime_error("Unsupported byte size unit suffix: '" + unitPart + "'");
    }
    if (baseSize > 0 && multiplier > std::numeric_limits<size_t>::max() / baseSize)
    {
        throw std::runtime_error("Calculated client_max_body_size exceeds physical address space limits: " + value);
    }
    return baseSize * multiplier;
}

std::string ConfigParser::parseErrorPagePathValue(const std::string &raw)
{
    std::string path = mergeSlashes(raw);

    if (path.empty())
        throw std::runtime_error("Invalid error_page path");

    if (path.find("..") != std::string::npos)
        throw std::runtime_error("Invalid error_page path");

    if (path[0] != '/')
        path = "/" + path;

    return path;
}

std::string ConfigParser::parseCgiExtValue(const std::string &raw)
{
    std::string ext = raw;

    if (ext.empty())
        throw std::runtime_error("Invalid cgi_ext");

    if (ext.find("..") != std::string::npos || ext.find("/") != std::string::npos)
        throw std::runtime_error("Invalid cgi_ext");

    if (ext[0] != '.')
        ext = "." + ext;

    if (ext.size() == 1)
        throw std::runtime_error("Invalid cgi_ext");

    return ext;
}

std::string ConfigParser::parseRedirectTargetValue(const std::string &target)
{
    if (target.empty())
        throw std::runtime_error("Missing redirect target");

    if (target.find("..") != std::string::npos)
        throw std::runtime_error("Invalid redirect target");

    if (target[0] != '/' && target.find("http://") != 0  &&
        target.find("https://") != 0)
        throw std::runtime_error("redirect target must be path or URL");

    return target;
}

std::string ConfigParser::parseFilesystemPath()
{
    std::string root = tokens.expect("Missing root");

    root = mergeSlashes(root);

    while (root.size() > 1 && root[root.size() - 1] == '/')
        root.erase(root.size() - 1);

    if (root.empty())
        throw std::runtime_error("Invalid root path");

    if (root.find("..") != std::string::npos)
        throw std::runtime_error("Invalid root path: directory traversal");

    return root;
}

std::string ConfigParser::parseLocationPath()
{
    std::string path = tokens.expect("Missing location path");

    path = mergeSlashes(path);

    if (path.empty() || path[0] != '/')
        throw std::runtime_error("Location path must start with '/'");

    while (path.size() > 1 && path[path.size() - 1] == '/')
        path.erase(path.size() - 1);

    if (path.find("..") != std::string::npos)
        throw std::runtime_error("Invalid location path: directory traversal");

    return path;
}


std::vector<std::string> ConfigParser::parseWordListUntilSemicolon(const std::string &directiveName)
{
    std::vector<std::string> values;

    while (tokens.hasMore() && tokens.current() != ";")
    {
        values.push_back(tokens.expect("Missing value for " + directiveName));
    }
    if (!tokens.hasMore())
        throw std::runtime_error("Missing ';' after " + directiveName);

    if (values.empty())
        throw std::runtime_error(directiveName + " directive requires at least one value");
    tokens.expectSemicolon(directiveName);
    return values;
}


std::vector<std::string> ConfigParser::parseIndexesList()
{
    std::vector<std::string> indexes = parseWordListUntilSemicolon("index");

    for (size_t i = 0; i < indexes.size(); ++i)
    {
        indexes[i] = mergeSlashes(indexes[i]);

        while (!indexes[i].empty() && indexes[i][0] == '/')
            indexes[i].erase(0, 1);

        if (indexes[i].empty())
            throw std::runtime_error("Index cannot be empty");

        if (indexes[i].find("..") != std::string::npos)
            throw std::runtime_error("Invalid index path: directory traversal");
    }

    return indexes;
}

std::string ConfigParser::parseCgiPathValue()
{
    std::string path = tokens.expect("Missing cgi_path");

    path = mergeSlashes(path);

    while (path.size() > 1 && path[path.size() - 1] == '/')
        path.erase(path.size() - 1);

    if (path.empty())
        throw std::runtime_error("Invalid cgi_path");

    if (path.find("..") != std::string::npos)
        throw std::runtime_error("Invalid cgi_path: directory traversal");
    return path;
}
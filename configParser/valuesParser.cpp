#include "valuesParser.hpp"
#include "../Helpers.hpp"
#include <cstdlib>
#include <sstream>
#include <stdexcept>

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
*/#include "valuesParser.hpp"
#include "../Helpers.hpp"
#include "../Errors.hpp"

#include <cctype>
#include <cstdlib>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace valuesParser
{

int parsePortValue(std::string value)
{
    if (!isOnlyDigits(value))
        throwError(ERR_INVALID_VALUE, value + " (must be numeric)");

    if (value.size() > 5)
        throwError(ERR_INVALID_VALUE, value + " (too long)");

    int port = std::atoi(value.c_str());

    if (port <= 0 || port > 65535)
        throwError(ERR_INVALID_VALUE, value + " (must be between 1 and 65535)");

    return port;
}

size_t parseBodySizeValue(TokenStream &tokens)
{
    std::string value = tokens.expectValue("client_max_body_size value").text;

    size_t position = 0;

    while (position < value.size() && std::isdigit(static_cast<unsigned char>(value[position])))
        ++position;

    if (position == 0)
        throwError(ERR_INVALID_VALUE, value + " (missing digits)");

    if (position < value.size() && value[position] == '.')
        throwError(ERR_INVALID_VALUE, value + " (decimals are not allowed)");

    std::string numericPart = value.substr(0, position);
    std::string unitPart = value.substr(position);

    size_t baseSize = 0;
    std::istringstream stream(numericPart);

    stream >> baseSize;

    if (stream.fail())
        throwError(ERR_INVALID_VALUE, numericPart + " (numeric overflow)");

    std::string unit = toUpper(trim(unitPart));
    size_t multiplier = 1;

    if (unit.empty() || unit == "B")
        multiplier = 1;
    else if (unit == "K" || unit == "KB")
        multiplier = 1024;
    else if (unit == "M" || unit == "MB")
        multiplier = 1024 * 1024;
    else if (unit == "G" || unit == "GB")
        multiplier = 1024 * 1024 * 1024;
    else
        throwError(ERR_INVALID_VALUE, unitPart + " (unsupported byte size unit suffix)");

    if (baseSize > 0 && multiplier > std::numeric_limits<size_t>::max() / baseSize)
        throwError(ERR_INVALID_VALUE, value + " (exceeds physical address space limits)");

    return baseSize * multiplier;
}

std::string parseErrorPagePathValue(TokenStream &tokens)
{
    std::string path = tokens.expectValue("error_page path").text;

    path = mergeSlashes(path);

    if (path.find("..") != std::string::npos)
        throwError(ERR_INVALID_VALUE, path + " (directory traversal not allowed)");

    if (path[0] != '/')
        path = "/" + path;

    return path;
}

std::string parseCgiExtValue(TokenStream &tokens)
{
    std::string extension = tokens.expectValue("cgi_ext value").text;

    if (extension.find("..") != std::string::npos || extension.find('/') != std::string::npos)
        throwError(ERR_INVALID_VALUE, extension + " (invalid characters in extension)");

    if (extension[0] != '.')
        extension = "." + extension;

    if (extension.size() == 1)
        throwError(ERR_INVALID_VALUE, extension + " (extension cannot be just '.')");

    return extension;
}

std::string parseRedirectTargetValue(TokenStream &tokens)
{
    std::string target = tokens.expectValue("redirect target").text;

    if (target.find("..") != std::string::npos)
        throwError(ERR_INVALID_VALUE, target + " (directory traversal not allowed)");

    if (target[0] != '/'
        && target.find("http://") != 0
        && target.find("https://") != 0)
    {
        throwError(ERR_INVALID_VALUE, target + " (must be absolute path or URL)");
    }

    return target;
}

std::string parseFilesystemPath(TokenStream &tokens)
{
    std::string path = tokens.expectValue("filesystem path").text;

    path = mergeSlashes(path);

    while (path.size() > 1 && path[path.size() - 1] == '/')
        path.erase(path.size() - 1);

    if (path.find("..") != std::string::npos)
        throwError(ERR_INVALID_VALUE, path + " (directory traversal not allowed)");

    return path;
}

std::string parseLocationPath(TokenStream &tokens)
{
    std::string path = tokens.expectValue("location path").text;

    path = mergeSlashes(path);

    if (path[0] != '/')
        throwError(ERR_INVALID_VALUE, path + " (must start with '/')");

    while (path.size() > 1 && path[path.size() - 1] == '/')
        path.erase(path.size() - 1);

    if (path.find("..") != std::string::npos)
        throwError(ERR_INVALID_VALUE, path + " (directory traversal not allowed)");

    return path;
}

std::vector<std::string> parseWordListUntilSemicolon(TokenStream &tokens, const std::string &directiveName)
{
    std::vector<std::string> values;

    while (!tokens.atEnd() && tokens.peek().text != ";")
        values.push_back(tokens.expectValue(directiveName + " value").text);

    if (tokens.atEnd())
        throwError(ERR_MISSING_SEMICOLON, directiveName);

    if (values.empty())
        throwError(ERR_MISSING_VALUE, directiveName + " (requires at least one value)");

    return values;
}

std::vector<std::string> parseIndexesList(TokenStream &tokens)
{
    std::vector<std::string> indexes = parseWordListUntilSemicolon(tokens, "index");

    for (size_t i = 0; i < indexes.size(); ++i)
    {
        indexes[i] = mergeSlashes(indexes[i]);

        while (!indexes[i].empty() && indexes[i][0] == '/')
            indexes[i].erase(0, 1);

        if (indexes[i].empty())
            throwError(ERR_INVALID_VALUE, "Empty index name");

        if (indexes[i].find("..") != std::string::npos)
            throwError(ERR_INVALID_VALUE, indexes[i] + " (directory traversal not allowed)");
    }

    return indexes;
}

std::string parseCgiPathValue(TokenStream &tokens)
{
    std::string path = tokens.expectValue("cgi_path value").text;

    path = mergeSlashes(path);

    while (path.size() > 1 && path[path.size() - 1] == '/')
        path.erase(path.size() - 1);

    if (path.find("..") != std::string::npos)
        throwError(ERR_INVALID_VALUE, path + " (directory traversal not allowed)");

    return path;
}

}
#include "valuesParser.hpp"
#include "../Helpers.hpp"
#include "../Errors.hpp"

#include <cctype>
#include <cstdlib>
#include <limits>
#include <sstream>

/*
===============================================================================
 PATH HANDLING RULES
===============================================================================

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

NORMALIZATION:

- Remove trailing slashes from filesystem and CGI paths.
- Preserve "/" as the root location path.
- Remove leading slashes from index values.
- Add a leading dot to CGI extensions when missing.
- Add a leading slash to error-page paths when missing.
- Collapse repeated slashes.

SECURITY:

- Reject paths containing "..".
- Do not allow paths to escape the configured root.
===============================================================================
*/

namespace valuesParser
{

    size_t parseBodySizeValue(TokenStream &tokens)
    {
        std::string value;
        size_t position;
        size_t baseSize;
        size_t multiplier;

        value = tokens.expectValue("client_max_body_size value").text;
        position = 0;

        while (position < value.size() && std::isdigit(static_cast<unsigned char>(value[position])))
        {
            ++position;
        }

        if (position == 0)
            throwConfigError(tokens, ERR_INVALID_BODY_SIZE);

        if (position < value.size() && value[position] == '.')
            throwConfigError(tokens, ERR_INVALID_BODY_SIZE);

        std::string numericPart = value.substr(0, position);
        std::string unit = toUpper(trim(value.substr(position)));

        baseSize = 0;

        std::istringstream stream(numericPart);
        stream >> baseSize;

        if (stream.fail() || !stream.eof())
            throwConfigError(tokens, ERR_INVALID_BODY_SIZE);

        multiplier = 1;

        if (unit.empty() || unit == "B")
            multiplier = 1;
        else if (unit == "K" || unit == "KB")
            multiplier = 1024;
        else if (unit == "M" || unit == "MB")
            multiplier = 1024 * 1024;
        else if (unit == "G" || unit == "GB")
            multiplier = 1024 * 1024 * 1024;
        else
            throwConfigError(tokens, ERR_INVALID_BODY_SIZE);

        if (baseSize > 0
            && multiplier > std::numeric_limits<size_t>::max() / baseSize)
        {
            throwConfigError(tokens, ERR_INVALID_BODY_SIZE);
        }

        return baseSize * multiplier;
    }

    std::string parseErrorPagePathValue(TokenStream &tokens)
    {
        std::string path;

        path = tokens.expectValue("error_page path").text;
        path = mergeSlashes(path);

        if (path.find("..") != std::string::npos)
            throwConfigError(tokens, ERR_INVALID_PATH);

        if (path[0] != '/')
            path = "/" + path;

        return path;
    }

    std::string parseCgiExtValue(TokenStream &tokens)
    {
        std::string extension;

        extension = tokens.expectValue("cgi_ext value").text;

        if (extension.find("..") != std::string::npos || extension.find('/') != std::string::npos)
        {
            throwConfigError(tokens, ERR_INVALID_CGI_EXTENSION);
        }

        if (extension[0] != '.')
            extension = "." + extension;

        if (extension.size() == 1)
            throwConfigError(tokens, ERR_INVALID_CGI_EXTENSION);

        return extension;
    }

    std::string parseRedirectTargetValue(TokenStream &tokens)
    {
        std::string target;

        target = tokens.expectValue("redirect target").text;

        if (target.find("..") != std::string::npos)
            throwConfigError(tokens, ERR_INVALID_REDIRECT_TARGET);

        if (target[0] != '/'
            && target.find("http://") != 0
            && target.find("https://") != 0)
        {
            throwConfigError(tokens, ERR_INVALID_REDIRECT_TARGET);
        }

        return target;
    }

    std::string parseFilesystemPath(TokenStream &tokens)
    {
        std::string path;

        path = tokens.expectValue("filesystem path").text;
        path = mergeSlashes(path);

        while (path.size() > 1 && path[path.size() - 1] == '/')
            path.erase(path.size() - 1);

        if (path.find("..") != std::string::npos)
            throwConfigError(tokens, ERR_INVALID_PATH);

        return path;
    }

    std::string parseLocationPath(TokenStream &tokens)
    {
        std::string path;

        path = tokens.expectValue("location path").text;
        path = mergeSlashes(path);

        if (path[0] != '/')
            throwConfigError(tokens, ERR_INVALID_PATH);

        while (path.size() > 1 && path[path.size() - 1] == '/')
            path.erase(path.size() - 1);

        if (path.find("..") != std::string::npos)
            throwConfigError(tokens, ERR_INVALID_PATH);

        return path;
    }

    std::vector<std::string> parseWordListUntilSemicolon(
        TokenStream &tokens,
        const std::string &directiveName
    )
    {
        std::vector<std::string> values;

        while (!tokens.atEnd() && tokens.peek().text != ";")
        {
            values.push_back(
                tokens.expectValue(directiveName + " value").text
            );
        }

        if (tokens.atEnd())
            throwConfigError(tokens, ERR_MISSING_SEMICOLON);

        if (values.empty())
            throwConfigError(tokens, ERR_MISSING_VALUE);

        return values;
    }

    std::vector<std::string> parseIndexesList(TokenStream &tokens)
    {
        std::vector<std::string> indexes;

        while (!tokens.atEnd() && tokens.peek().text != ";")
        {
            std::string index;

            index = tokens.expectValue("index value").text;
            index = mergeSlashes(index);

            while (!index.empty() && index[0] == '/')
                index.erase(0, 1);

            if (index.empty())
                throwConfigError(tokens, ERR_INVALID_PATH);

            if (index.find("..") != std::string::npos)
                throwConfigError(tokens, ERR_INVALID_PATH);

            indexes.push_back(index);
        }

        if (tokens.atEnd())
            throwConfigError(tokens, ERR_MISSING_SEMICOLON);

        if (indexes.empty())
            throwConfigError(tokens, ERR_MISSING_VALUE);

        return indexes;
    }

    std::string parseCgiPathValue(TokenStream &tokens)
    {
        std::string path;

        path = tokens.expectValue("cgi_path value").text;
        path = mergeSlashes(path);

        while (path.size() > 1 && path[path.size() - 1] == '/')
            path.erase(path.size() - 1);

        if (path.find("..") != std::string::npos)
            throwConfigError(tokens, ERR_INVALID_PATH);

        return path;
    }

}
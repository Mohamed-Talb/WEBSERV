#include "configParser.hpp"

#include <algorithm>




#include "configParser.hpp"


void ConfigParser::locationMethods(Location &loc)
{
    checkDuplicate(loc, "methods");
    tokens.expect("methods");

    bool hasMethod = false;

    while (!tokens.atEnd() && tokens.peekCurrent().text != ";")
    {
        const Token &methodToken = tokens.peekValue("HTTP method");
        std::string method = toUpper(methodToken.text);

        if (std::find(loc.allowedMethods.begin(),loc.allowedMethods.end(),method) == loc.allowedMethods.end())
        {
            throwConfigError(tokens, ERR_INVALID_METHOD);
        }
        if (std::find( loc.methods.begin(),loc.methods.end(),method) != loc.methods.end())
        {
            throwConfigError(tokens, ERR_DUPLICATE_VALUE);
        }
        loc.methods.push_back(method);
        tokens.consume();
        hasMethod = true;
    }

    if (!hasMethod)
        throwConfigError(tokens, ERR_MISSING_VALUE);

    if (tokens.atEnd())
        throwConfigError(tokens, ERR_MISSING_SEMICOLON);

    tokens.expect(";");
}

void ConfigParser::locationRoot(Location &loc)
{
    checkDuplicate(loc, "root");
    tokens.expect("root");

    loc.root = valuesParser::parseFilesystemPath(tokens);

    tokens.expect(";");
}

void ConfigParser::locationAutoindex(Location &loc)
{
    checkDuplicate(loc, "autoindex");
    tokens.expect("autoindex");

    const Token &valueToken = tokens.peekValue("autoindex value");

    if (valueToken.text != "on" && valueToken.text != "off")
        throwConfigError(tokens, ERR_EXPECTED_ON_OFF);

    loc.autoindex = valueToken.text;
    tokens.consume();

    tokens.expect(";");
}

void ConfigParser::locationIndex(Location &loc)
{
    checkDuplicate(loc, "index");
    tokens.expect("index");

    loc.indexes = valuesParser::parseIndexesList(tokens);

    tokens.expect(";");
}

void ConfigParser::locationClientMaxBodySize(Location &loc)
{
    checkDuplicate(loc, "client_max_body_size");
    tokens.expect("client_max_body_size");

    loc.client_max_body_size = valuesParser::parseBodySizeValue(tokens);

    tokens.expect(";");
}

void ConfigParser::locationCgiMapping(Location &loc)
{
    tokens.expect("cgi_pass");

    const Token &extToken = tokens.peekValue("cgi extension");
    std::string extension = extToken.text;

    if (extension.empty() || extension.find("..") != std::string::npos || extension.find('/') != std::string::npos)
        throwConfigError(tokens, ERR_INVALID_EXTENSION);

    if (extension[0] != '.')
        extension = "." + extension;

    if (extension.size() == 1)
        throwConfigError(tokens, ERR_INVALID_EXTENSION);

    for (size_t i = 1; i < extension.size(); ++i)
    {
        unsigned char c = static_cast<unsigned char>(extension[i]);

        if (!std::isalnum(c) && c != '_' && c != '-')
            throwConfigError(tokens, ERR_INVALID_EXTENSION);
    }

    tokens.consume();

    const Token &pathToken = tokens.peekValue("cgi path");
    std::string path = mergeSlashes(pathToken.text);

    if (path.empty() || path.find("..") != std::string::npos)
        throwConfigError(tokens, ERR_INVALID_PATH);

    while (path.size() > 1 && path[path.size() - 1] == '/')
        path.erase(path.size() - 1);

    tokens.consume();

    if (loc.cgiMappings.find(extension) != loc.cgiMappings.end())
        throwConfigError(tokens, ERR_DUPLICATE_VALUE);

    loc.cgiMappings[extension] = path;

    tokens.expect(";");
}

void ConfigParser::locationRedirect(Location &loc)
{
    checkDuplicate(loc, "return");
    tokens.expect("return");

    const Token &codeToken = tokens.peekValue("redirect status code");

    if (!isOnlyDigits(codeToken.text))
        throwConfigError(tokens, ERR_INVALID_REDIRECT_CODE);

    int redirectCode = std::atoi(codeToken.text.c_str());

    if (redirectCode != 301 && redirectCode != 302)
        throwConfigError(tokens, ERR_INVALID_REDIRECT_CODE);

    loc.redirectCode = redirectCode;
    tokens.consume();

    loc.redirectTarget =
        valuesParser::parseRedirectTargetValue(tokens);

    tokens.expect(";");
}

void ConfigParser::locationUpload(Location &loc)
{
    checkDuplicate(loc, "upload");
    tokens.expect("upload");

    const Token &valueToken = tokens.peekValue("upload value");

    if (valueToken.text != "on" && valueToken.text != "off")
        throwConfigError(tokens, ERR_EXPECTED_ON_OFF);

    loc.uploadEnabled = valueToken.text;
    tokens.consume();

    tokens.expect(";");
}

void ConfigParser::locationUploadPath(Location &loc)
{
    checkDuplicate(loc, "upload_path");
    tokens.expect("upload_path");

    loc.uploadPath = valuesParser::parseFilesystemPath(tokens);

    tokens.expect(";");
}


#include "configParser.hpp"

void ConfigParser::serverListen(ServerConfig &conf)
{
    checkDuplicate(conf, "listen");
    tokens.expect("listen");

    const Token &valueToken = tokens.peekValue("listen value");
    std::string value = valueToken.text;

    if (value.find(':') == std::string::npos)
    {
        if (value.find('.') != std::string::npos || value == "localhost")
        {
            value += ":80";
        }
        else
        {
            value = "0.0.0.0:" + value;
        }
    }

    size_t colonPosition = value.find(':');

    if (colonPosition == 0 || colonPosition == value.size() - 1 || value.find(':', colonPosition + 1) != std::string::npos)
    {
        throwConfigError(tokens, ERR_INVALID_LISTEN);
    }

    std::string host = value.substr(0, colonPosition);
    std::string portValue = value.substr(colonPosition + 1);

    if (host == "localhost")
        host = "127.0.0.1";

    if (!isValidHost(host))
        throwConfigError(tokens, ERR_INVALID_HOST);

    if (!isOnlyDigits(portValue) || portValue.size() > 5)
        throwConfigError(tokens, ERR_INVALID_PORT);

    int port = std::atoi(portValue.c_str());

    if (port <= 0 || port > 65535)
        throwConfigError(tokens, ERR_INVALID_PORT);

    conf.host = host;
    conf.port = port;

    tokens.consume();
    tokens.expect(";");
}

void ConfigParser::serverRoot(ServerConfig &conf)
{
    checkDuplicate(conf, "root");
    tokens.expect("root");

    conf.root = valuesParser::parseFilesystemPath(tokens);
    std::cout << conf.root << std::endl;
    tokens.expect(";");
}

void ConfigParser::serverNames(ServerConfig &conf)
{
    checkDuplicate(conf, "server_name");
    tokens.expect("server_name");

    conf.serverNames.clear();

    while (!tokens.atEnd() && tokens.peekCurrent().text != ";")
    {
        const Token &nameToken = tokens.peekValue("server name");

        if (!isValidServerName(nameToken.text))
            throwConfigError(tokens, ERR_INVALID_SERVER_NAME);

        conf.serverNames.push_back(nameToken.text);
        tokens.consume();
    }

    if (conf.serverNames.empty())
        throwConfigError(tokens, ERR_MISSING_VALUE);

    if (tokens.atEnd())
        throwConfigError(tokens, ERR_MISSING_SEMICOLON);

    tokens.expect(";");
}

void ConfigParser::serverIndex(ServerConfig &conf)
{
    checkDuplicate(conf, "index");
    tokens.expect("index");

    conf.indexes = valuesParser::parseIndexesList(tokens);
    tokens.expect(";");
}

void ConfigParser::serverClientMaxBodySize(ServerConfig &conf)
{
    checkDuplicate(conf, "client_max_body_size");
    tokens.expect("client_max_body_size");

    conf.client_max_body_size =
        valuesParser::parseBodySizeValue(tokens);

    tokens.expect(";");
}

void ConfigParser::serverErrorPages(ServerConfig &conf)
{
    tokens.expect("error_page");

    std::vector<int> codes;
    while (!tokens.atEnd() && tokens.peekCurrent().text != ";" && isOnlyDigits(tokens.peekCurrent().text))
    {
        const Token &codeToken = tokens.peekValue("error status code");

        int errorCode = 0;
        std::istringstream stream(codeToken.text);
        stream >> errorCode;
        if (stream.fail() || !stream.eof() || !isValidErrorCode(errorCode))
        {
            throwConfigError(tokens, ERR_INVALID_STATUS_CODE);
        }
        if (std::find(codes.begin(), codes.end(), errorCode) != codes.end())
        {
            throwConfigError(tokens,ERR_DUPLICATE_VALUE);
        }
        if (conf.errorPage.count(errorCode) != 0)
        {
            throwConfigError(tokens,ERR_DUPLICATE_VALUE);
        }
        codes.push_back(errorCode);
        tokens.consume();
    }

    if (codes.empty())
        throwConfigError(tokens, ERR_MISSING_VALUE);

    if (tokens.atEnd() || tokens.peekCurrent().text == ";")
        throwConfigError(tokens, ERR_MISSING_VALUE);

    std::string path =
        valuesParser::parseErrorPagePathValue(tokens);

    tokens.expect(";");

    for (size_t i = 0; i < codes.size(); ++i)
        conf.errorPage[codes[i]] = path;
}


void ConfigParser::serverLocation(ServerConfig &conf)
{
    tokens.expect("location");

    std::string path =
        valuesParser::parseLocationPath(tokens);

    for (size_t i = 0; i < conf.locations.size(); ++i)
    {
        if (conf.locations[i].path == path)
            throwConfigError(tokens, ERR_DUPLICATE_LOCATION);
    }

    Location location;
    location.path = path;

    tokens.consume();
    tokens.expect("{");

    while (!tokens.atEnd())
    {
        if (tokens.peekCurrent().text == "}")
        {
            validateLocation(location);
            tokens.expect("}");

            conf.locations.push_back(location);
            return;
        }

        const std::string &directive =
            tokens.peekCurrent().text;

        std::map<std::string, LocationHandler>::iterator handler;
        handler = locationDispatch.find(directive);

        if (handler == locationDispatch.end())
        {
            throwConfigError(tokens,ERR_UNKNOWN_DIRECTIVE);
        }
        (this->*(handler->second))(location);
    }
    throwConfigError(tokens, ERR_UNCLOSED_LOCATION);
}





void ConfigParser::validateLocation(Location &loc)
{
    if (loc.uploadEnabled == "on" && loc.uploadPath.empty())
    {
        throw std::runtime_error(
            "Config error in location '" + loc.path
            + "': upload is enabled but upload_path is missing"
        );
    }

    if (loc.uploadEnabled == "off" && !loc.uploadPath.empty())
    {
        throw std::runtime_error(
            "Config error in location '" + loc.path
            + "': upload_path is set but upload is disabled"
        );
    }
}

void ConfigParser::checkDuplicate(ServerConfig &conf, const std::string &directive) const
{
    if (conf.seenDirectives.find(directive) != conf.seenDirectives.end())
        throwConfigError(tokens, ERR_DUPLICATE_DIRECTIVE);

    conf.seenDirectives[directive] = true;
}

void ConfigParser::checkDuplicate(Location &loc, const std::string &directive) const
{
    if (loc.seenDirectives.find(directive) != loc.seenDirectives.end())
        throwConfigError(tokens, ERR_DUPLICATE_DIRECTIVE);

    loc.seenDirectives[directive] = true;
}

void ConfigParser::validateServer(ServerConfig &conf)
{
    for (size_t i = 0; i < conf.locations.size(); ++i)
    {
        Location &loc = conf.locations[i];

        if (loc.root.empty())
            loc.root = conf.root;

        if (loc.indexes.empty())
            loc.indexes = conf.indexes;
        
        if (loc.client_max_body_size == -1)
            loc.client_max_body_size = conf.client_max_body_size;
    }
}

void ConfigParser::initServerDispatch()
{
    serverDispatch["listen"] = &ConfigParser::serverListen;
    serverDispatch["root"] = &ConfigParser::serverRoot;
    serverDispatch["server_name"] = &ConfigParser::serverNames;
    serverDispatch["location"] = &ConfigParser::serverLocation;
    serverDispatch["index"] = &ConfigParser::serverIndex;
    serverDispatch["error_page"] = &ConfigParser::serverErrorPages;
    serverDispatch["client_max_body_size"] = &ConfigParser::serverClientMaxBodySize;
}

void ConfigParser::initLocationDispatch()
{
    locationDispatch["methods"] = &ConfigParser::locationMethods;
    locationDispatch["root"] = &ConfigParser::locationRoot;
    locationDispatch["autoindex"] = &ConfigParser::locationAutoindex;
    locationDispatch["index"] = &ConfigParser::locationIndex;
    locationDispatch["cgi_pass"] = &ConfigParser::locationCgiMapping;
    locationDispatch["return"] = &ConfigParser::locationRedirect;
    locationDispatch["upload"] = &ConfigParser::locationUpload;
    locationDispatch["upload_path"] = &ConfigParser::locationUploadPath;
    locationDispatch["client_max_body_size"] = &ConfigParser::locationClientMaxBodySize;
}

ConfigParser::ConfigParser()
{
    initServerDispatch();
    initLocationDispatch();
}

void ConfigParser::parseServerBlock(ServerConfig &conf)
{
    tokens.expect("server");
    tokens.expect("{");

    while (!tokens.atEnd())
    {
        if (tokens.peekCurrent().text == "}")
        {
            std::sort(conf.locations.begin(), conf.locations.end(), CompareLocations());
            validateServer(conf);
            tokens.expect("}");
            return;
        }
        const std::string &directive = tokens.peekCurrent().text;

        std::map<std::string, ServerHandler>::iterator handler;
        handler = serverDispatch.find(directive);

        if (handler == serverDispatch.end())
            throwConfigError(tokens, ERR_UNKNOWN_DIRECTIVE);

        (this->*(handler->second))(conf);
    }
    throwConfigError(tokens, ERR_UNCLOSED_SERVER);
}

std::vector<ServerConfig> ConfigParser::loadeConfig(std::string configFile)
{
    tokens = TokenStream(configFile);
    std::vector<ServerConfig> servers;

    if (tokens.atEnd())
        throwConfigError(tokens, ERR_EMPTY_CONFIG);

    while (!tokens.atEnd())
    {
        ServerConfig conf;

        parseServerBlock(conf);
        servers.push_back(conf);
    }

    return servers;
}


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

#include "valuesParser.hpp"
#include "../Helpers.hpp"
#include "configError.hpp"

namespace valuesParser
{

    size_t parseBodySizeValue(TokenStream &tokens)
    {
        const Token &valueToken =
            tokens.peekValue("client_max_body_size value");

        const std::string &value = valueToken.text;
        size_t position = 0;

        while (position < value.size() && std::isdigit(static_cast<unsigned char>(value[position])))
        {
            ++position;
        }

        if (position == 0)
            throwConfigError(tokens, ERR_INVALID_SIZE);

        if (position < value.size() && value[position] == '.')
            throwConfigError(tokens, ERR_INVALID_SIZE);

        std::string numericPart = value.substr(0, position);
        std::string unit = toUpper(trim(value.substr(position)));

        size_t baseSize = 0;
        std::istringstream stream(numericPart);

        stream >> baseSize;

        if (stream.fail() || !stream.eof())
            throwConfigError(tokens, ERR_INVALID_SIZE);

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
            throwConfigError(tokens, ERR_INVALID_SIZE);

        if (baseSize == 0)
            throwConfigError(tokens, ERR_INVALID_SIZE);

        if (multiplier
            > std::numeric_limits<size_t>::max() / baseSize)
        {
            throwConfigError(tokens, ERR_INVALID_SIZE);
        }

        size_t result = baseSize * multiplier;
        tokens.consume();
        return result;
    }

    std::string parseErrorPagePathValue(TokenStream &tokens)
    {
        const Token &pathToken =
            tokens.peekValue("error_page path");

        std::string path = mergeSlashes(pathToken.text);

        if (path.find("..") != std::string::npos)
            throwConfigError(tokens, ERR_INVALID_PATH);

        if (path.empty() || path[0] != '/')
            path = "/" + path;

        tokens.consume();

        return path;
    }

    std::string parseRedirectTargetValue(TokenStream &tokens)
    {
        const Token &targetToken =
            tokens.peekValue("redirect target");

        const std::string &target = targetToken.text;

        if (target.find("..") != std::string::npos)
            throwConfigError(tokens, ERR_INVALID_REDIRECT_TARGET);

        if (target.empty() || (target[0] != '/' && target.find("http://") != 0 && target.find("https://") != 0))
        {
            throwConfigError(tokens, ERR_INVALID_REDIRECT_TARGET);
        }

        std::string result = target;
        tokens.consume();
        return result;
    }

    std::string parseFilesystemPath(TokenStream &tokens)
    {
        const Token &pathToken = tokens.peekValue("filesystem path");

        std::string path = mergeSlashes(pathToken.text);
        while (path.size() > 1 && path[path.size() - 1] == '/')
        {
            path.erase(path.size() - 1);
        }

        if (path.find("..") != std::string::npos)
            throwConfigError(tokens, ERR_INVALID_PATH);

        tokens.consume();

        return path;
    }

    std::string parseLocationPath(TokenStream &tokens)
    {
        const Token &pathToken =
            tokens.peekValue("location path");

        std::string path = mergeSlashes(pathToken.text);

        if (path.empty() || path[0] != '/')
            throwConfigError(tokens, ERR_INVALID_PATH);

        while (path.size() > 1
            && path[path.size() - 1] == '/')
        {
            path.erase(path.size() - 1);
        }

        if (path.find("..") != std::string::npos)
            throwConfigError(tokens, ERR_INVALID_PATH);

        return path;
    }

    std::vector<std::string> parseWordListUntilSemicolon(TokenStream &tokens,const std::string &directiveName)
    {
        std::vector<std::string> values;

        while (!tokens.atEnd() && tokens.peekCurrent().text != ";")
        {
            const Token &valueToken =
                tokens.peekValue(directiveName + " value");

            values.push_back(valueToken.text);
            tokens.consume();
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

        while (!tokens.atEnd() && tokens.peekCurrent().text != ";")
        {
            const Token &indexToken =
                tokens.peekValue("index value");

            std::string index = mergeSlashes(indexToken.text);

            while (!index.empty() && index[0] == '/')
                index.erase(0, 1);

            if (index.empty()
                || index.find("..") != std::string::npos)
            {
                throwConfigError(tokens, ERR_INVALID_PATH);
            }

            indexes.push_back(index);
            tokens.consume();
        }

        if (tokens.atEnd())
            throwConfigError(tokens, ERR_MISSING_SEMICOLON);

        if (indexes.empty())
            throwConfigError(tokens, ERR_MISSING_VALUE);

        return indexes;
    }
}


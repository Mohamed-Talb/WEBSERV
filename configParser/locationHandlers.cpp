#include "configParser.hpp"
#include "../Errors.hpp"

#include <algorithm>
#include <cstdlib>

void ConfigParser::locationMethods(Location &loc)
{
    checkDuplicate(loc, "methods");
    tokens.expect("methods");

    bool hasMethod = false;

    while (!tokens.atEnd() && tokens.peek().text != ";")
    {
        std::string currentMethod = toUpper(tokens.expectValue("HTTP method").text);
        hasMethod = true;

        if (std::find(loc.allowedMethods.begin(), loc.allowedMethods.end(), currentMethod) == loc.allowedMethods.end())
            throwConfigError(tokens, ERR_INVALID_METHOD);

        if (std::find(loc.methods.begin(), loc.methods.end(), currentMethod) != loc.methods.end())
            throwConfigError(tokens, ERR_DUPLICATE_METHOD);

        loc.methods.push_back(currentMethod);
    }

    if (!hasMethod)
        throwConfigError(tokens, ERR_MISSING_VALUE);

    if (tokens.atEnd())
        throwConfigError(tokens, ERR_MISSING_SEMICOLON);

    tokens.expectSemicolon();
}

void ConfigParser::locationRoot(Location &loc)
{
    checkDuplicate(loc, "root");
    tokens.expect("root");

    loc.root = valuesParser::parseFilesystemPath(tokens);

    tokens.expectSemicolon();
}

void ConfigParser::locationAutoindex(Location &loc)
{
    checkDuplicate(loc, "autoindex");
    tokens.expect("autoindex");

    if (tokens.atEnd() || tokens.peek().text == ";"
        || tokens.peek().text == "{" || tokens.peek().text == "}")
    {
        throwConfigError(tokens, ERR_MISSING_VALUE);
    }

    loc.autoindex = tokens.expectValue("autoindex value").text;

    if (loc.autoindex != "on" && loc.autoindex != "off")
        throwConfigError(tokens, ERR_INVALID_AUTOINDEX);

    tokens.expectSemicolon();
}

void ConfigParser::locationIndex(Location &loc)
{
    checkDuplicate(loc, "index");
    tokens.expect("index");

    loc.indexes = valuesParser::parseIndexesList(tokens);

    tokens.expectSemicolon();
}

void ConfigParser::locationCgiPath(Location &loc)
{
    checkDuplicate(loc, "cgi_path");
    tokens.expect("cgi_path");

    loc.cgiPath = valuesParser::parseCgiPathValue(tokens);

    tokens.expectSemicolon();
}

void ConfigParser::locationCgiExt(Location &loc)
{
    checkDuplicate(loc, "cgi_ext");
    tokens.expect("cgi_ext");

    loc.cgiExt = valuesParser::parseCgiExtValue(tokens);

    tokens.expectSemicolon();
}

void ConfigParser::locationRedirect(Location &loc)
{
    checkDuplicate(loc, "return");
    tokens.expect("return");

    if (tokens.atEnd() || tokens.peek().text == ";"
        || tokens.peek().text == "{" || tokens.peek().text == "}")
    {
        throwConfigError(tokens, ERR_MISSING_VALUE);
    }

    std::string codeValue = tokens.expectValue("redirect status code").text;

    if (!isOnlyDigits(codeValue))
        throwConfigError(tokens, ERR_INVALID_REDIRECT_CODE);

    loc.redirectCode = std::atoi(codeValue.c_str());

    if (loc.redirectCode != 301 && loc.redirectCode != 302)
        throwConfigError(tokens, ERR_INVALID_REDIRECT_CODE);

    if (tokens.atEnd() || tokens.peek().text == ";"
        || tokens.peek().text == "{" || tokens.peek().text == "}")
    {
        throwConfigError(tokens, ERR_MISSING_VALUE);
    }

    loc.redirectTarget = valuesParser::parseRedirectTargetValue(tokens);

    tokens.expectSemicolon();
}

void ConfigParser::locationUpload(Location &loc)
{
    checkDuplicate(loc, "upload");
    tokens.expect("upload");

    if (tokens.atEnd() || tokens.peek().text == ";" || tokens.peek().text == "{" || tokens.peek().text == "}")
    {
        throwConfigError(tokens, ERR_MISSING_VALUE);
    }
    loc.uploadEnabled = tokens.expectValue("upload value").text;

    if (loc.uploadEnabled != "on" && loc.uploadEnabled != "off")
        throwConfigError(tokens, ERR_INVALID_UPLOAD);

    tokens.expectSemicolon();
}

void ConfigParser::locationUploadPath(Location &loc)
{
    checkDuplicate(loc, "upload_path");
    tokens.expect("upload_path");

    loc.uploadPath = valuesParser::parseFilesystemPath(tokens);

    tokens.expectSemicolon();
}
#include "configParser.hpp"
#include "../Errors.hpp"

void ConfigParser::locationMethods(Location &loc)
{
    checkDuplicate(loc, "methods");
    tokens.expect("methods");

    std::vector<std::string> methods = valuesParser::parseWordListUntilSemicolon(tokens, "methods");

    for (size_t i = 0; i < methods.size(); ++i)
    {
        std::string currentMethod = toUpper(methods[i]);

        if (std::find(loc.allowedMethods.begin(), loc.allowedMethods.end(), currentMethod) == loc.allowedMethods.end())
            throwError(ERR_INVALID_VALUE, currentMethod);

        if (std::find(loc.methods.begin(), loc.methods.end(), currentMethod) != loc.methods.end())
            throwError(ERR_DUPLICATE_VALUE, currentMethod);

        loc.methods.push_back(currentMethod);
    }

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

    loc.autoindex = tokens.expectValue("autoindex value").text;

    if (loc.autoindex != "on" && loc.autoindex != "off")
        throwError(ERR_INVALID_VALUE, loc.autoindex);

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

    std::string codeValue = tokens.expectValue("redirect status code").text;

    if (!isOnlyDigits(codeValue))
        throwError(ERR_INVALID_VALUE, codeValue);

    loc.redirectCode = std::atoi(codeValue.c_str());

    if (loc.redirectCode != 301 && loc.redirectCode != 302)
        throwError(ERR_INVALID_VALUE, codeValue);

    loc.redirectTarget = valuesParser::parseRedirectTargetValue(tokens);

    tokens.expectSemicolon();
}

void ConfigParser::locationUpload(Location &loc)
{
    checkDuplicate(loc, "upload");
    tokens.expect("upload");

    loc.uploadEnabled = tokens.expectValue("upload value").text;

    if (loc.uploadEnabled != "on" && loc.uploadEnabled != "off")
        throwError(ERR_INVALID_VALUE, loc.uploadEnabled);

    tokens.expectSemicolon();
}

void ConfigParser::locationUploadPath(Location &loc)
{
    checkDuplicate(loc, "upload_path");
    tokens.expect("upload_path");
    loc.uploadPath = valuesParser::parseFilesystemPath(tokens);
    tokens.expectSemicolon();
}
#include "configParser.hpp"
#include "../Errors.hpp"

void ConfigParser::locationMethods(Location &loc)
{
    this->checkDuplicate(loc, "methods");
    tokens.expect("Expected methods");
    std::vector<std::string> methods = valuesParser::parseWordListUntilSemicolon(tokens, "methods");
    for (size_t i = 0; i < methods.size(); ++i)
    {
        std::string currMethod = toUpper(methods[i]);
        if (std::find(loc.allowedMethods.begin(), loc.allowedMethods.end(), currMethod) == loc.allowedMethods.end())
        {
            throwError(ERR_INVALID_VALUE, currMethod);
        }
        if (std::find(loc.methods.begin(), loc.methods.end(), currMethod) != loc.methods.end())
        {
            throwError(ERR_DUPLICATE_VALUE, currMethod);
        }
        loc.methods.push_back(currMethod);
    }
    tokens.expectSemicolon("methods");
}

void ConfigParser::locationRoot(Location &loc)
{
    this->checkDuplicate(loc, "root");
    
    tokens.expect("Expected root");
    loc.root = valuesParser::parseFilesystemPath(tokens);
    tokens.expectSemicolon("root");
}

void ConfigParser::locationAutoindex(Location &loc)
{
    this->checkDuplicate(loc, "autoindex");
    tokens.expect("Expected autoindex");
    loc.autoindex = tokens.expect("Missing autoindex value");
    if (loc.autoindex != "on" && loc.autoindex != "off")
        throwError(ERR_INVALID_VALUE, loc.autoindex);

    tokens.expectSemicolon("autoindex");
}

void ConfigParser::locationIndex(Location &loc)
{
    this->checkDuplicate(loc, "index");
    
    tokens.expect("Expected index");
    loc.indexes = valuesParser::parseIndexesList(tokens);
    tokens.expectSemicolon("index"); 
}

void ConfigParser::locationCgiPath(Location &loc)
{
    this->checkDuplicate(loc, "cgi_path");

    tokens.expect("Expected cgi_path");
    loc.cgiPath = valuesParser::parseCgiPathValue(tokens);
    tokens.expectSemicolon("cgi_path");
}

void ConfigParser::locationCgiExt(Location &loc)
{
    this->checkDuplicate(loc, "cgi_ext");
    
    tokens.expect("Expected cgi_ext");
    loc.cgiExt = valuesParser::parseCgiExtValue(tokens);
    tokens.expectSemicolon("cgi_ext");
}

void ConfigParser::locationRedirect(Location &loc)
{
    this->checkDuplicate(loc, "redirect");

    tokens.expect("Expected redirect");

    std::string codeValue = tokens.expect("Missing redirect code");

    if (!isOnlyDigits(codeValue))
        throwError(ERR_INVALID_VALUE, codeValue);

    loc.redirectCode = std::atoi(codeValue.c_str());

    if (loc.redirectCode != 301 && loc.redirectCode != 302)
        throwError(ERR_INVALID_VALUE, codeValue);

    loc.redirectTarget = valuesParser::parseRedirectTargetValue(tokens);

    tokens.expectSemicolon("redirect");
}

void ConfigParser::locationUpload(Location &loc)
{
    this->checkDuplicate(loc, "upload");
    
    tokens.expect("Expected upload");

    loc.uploadEnabled = tokens.expect("Missing upload value");

    if (loc.uploadEnabled != "on" && loc.uploadEnabled != "off")
        throwError(ERR_INVALID_VALUE, loc.uploadEnabled);

    tokens.expectSemicolon("upload");
}

void ConfigParser::locationUploadPath(Location &loc)
{
    this->checkDuplicate(loc, "upload_path");
    tokens.expect("Expected upload_path");
    loc.uploadPath = valuesParser::parseFilesystemPath(tokens);
    tokens.expectSemicolon("upload_path");
}
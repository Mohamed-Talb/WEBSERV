#include "configParser.hpp"

void ConfigParser::locationMethods(Location &loc)
{
    if (loc.seenDirectives["methods"])
        throw std::runtime_error("duplicate methods directive");

    loc.seenDirectives["methods"] = true;

    tokens.expect("Expected methods");

    std::vector<std::string> methods = valuesParser::parseWordListUntilSemicolon(tokens, "methods");
    for (size_t i = 0; i < methods.size(); ++i)
    {
        std::string currMethod = toUpper(methods[i]);
        if (std::find(loc.allowedMethods.begin(),loc.allowedMethods.end(),currMethod) == loc.allowedMethods.end())
        {
            throw std::runtime_error("unsupported method: " + currMethod);
        }
        if (std::find(loc.methods.begin(),loc.methods.end(),currMethod) != loc.methods.end())
        {
            throw std::runtime_error("duplicate method: " + currMethod);
        }
        loc.methods.push_back(currMethod);
    }
}

void ConfigParser::locationRoot(Location &loc)
{
    if (loc.seenDirectives["root"])
        throw std::runtime_error("duplicate root directive in location");

    loc.seenDirectives["root"] = true;
    tokens.expect("Expected root");

    loc.root = valuesParser::parseFilesystemPath(tokens);

    tokens.expectSemicolon("root");
}

void ConfigParser::locationAutoindex(Location &loc)
{
    if (loc.seenDirectives["autoindex"])
        throw std::runtime_error("duplicate autoindex directive");

    loc.seenDirectives["autoindex"] = true;
    tokens.expect("Expected autoindex");

    loc.autoindex = tokens.expect("Missing autoindex value");

    if (loc.autoindex != "on" && loc.autoindex != "off")
        throw std::runtime_error("autoindex must be 'on' or 'off'");

    tokens.expectSemicolon("autoindex");
}

void ConfigParser::locationIndex(Location &loc)
{
    if (loc.seenDirectives["index"])
        throw std::runtime_error("duplicate index directive in location");

    loc.seenDirectives["index"] = true;
    tokens.expect("Expected index");
    loc.indexes = valuesParser::parseIndexesList(tokens);
}

void ConfigParser::locationCgiPath(Location &loc)
{
    if (loc.seenDirectives["cgi_path"])
        throw std::runtime_error("duplicate cgi_path directive");

    loc.seenDirectives["cgi_path"] = true;

    tokens.expect("Expected cgi_path");

    loc.cgiPath = valuesParser::parseCgiPathValue(tokens);

    tokens.expectSemicolon("cgi_path");
}

void ConfigParser::locationCgiExt(Location &loc)
{
    if (loc.seenDirectives["cgi_ext"])
        throw std::runtime_error("duplicate cgi_ext directive");

    loc.seenDirectives["cgi_ext"] = true;
    tokens.expect("Expected cgi_ext");

    loc.cgiExt = valuesParser::parseCgiExtValue(tokens);

    tokens.expectSemicolon("cgi_ext");
}

void ConfigParser::locationRedirect(Location &loc)
{
    if (loc.seenDirectives["redirect"])
        throw std::runtime_error("duplicate redirect directive");

    loc.seenDirectives["redirect"] = true;
    if (loc.redirectCode != 0)
        throw std::runtime_error("duplicate redirect directive");

    tokens.expect("Expected redirect");

    std::string codeValue = tokens.expect("Missing redirect code");

    if (!isOnlyDigits(codeValue))
        throw std::runtime_error("Invalid redirect code");

    loc.redirectCode = std::atoi(codeValue.c_str());

    if (loc.redirectCode != 301 && loc.redirectCode != 302)
        throw std::runtime_error("Invalid redirect code");

    loc.redirectTarget = valuesParser::parseRedirectTargetValue(tokens);

    tokens.expectSemicolon("redirect");
}

void ConfigParser::locationUpload(Location &loc)
{
    if (loc.seenDirectives["upload"])
        throw std::runtime_error("duplicate upload directive");

    loc.seenDirectives["upload"] = true;
    tokens.expect("Expected upload");

    loc.uploadEnabled = tokens.expect("Missing upload value");

    if (loc.uploadEnabled != "on" && loc.uploadEnabled != "off")
        throw std::runtime_error("upload must be 'on' or 'off'");

    tokens.expectSemicolon("upload");
}

void ConfigParser::locationUploadPath(Location &loc)
{
    if (loc.seenDirectives["upload_path"])
        throw std::runtime_error("duplicate upload_path directive");

    loc.seenDirectives["upload_path"] = true;
    if (!loc.uploadPath.empty())
        throw std::runtime_error("duplicate upload_path directive");

    tokens.expect("Expected upload_path");

    loc.uploadPath = valuesParser::parseFilesystemPath(tokens);

    tokens.expectSemicolon("upload_path");
}
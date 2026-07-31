
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
    checkDuplicate(loc, "cgi");
    tokens.expect("cgi");
    
    loc.cgiMappings = valuesParser::parseCgiMappings(tokens);
    
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
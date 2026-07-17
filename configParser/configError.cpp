#include "./Tokenize/tokenStream.hpp"
#include "./configError.hpp"
#include <sstream>
std::string getErrorMessage(int errorCode)
{
    switch (errorCode)
    {
        case ERR_EMPTY_CONFIG:
            return "configuration file is empty";
        case ERR_UNKNOWN_DIRECTIVE:
            return "unknown directive";
        case ERR_DUPLICATE_DIRECTIVE:
            return "duplicate directive";
        case ERR_DUPLICATE_LISTEN:
            return "only one listen directive is allowed per server block";
        case ERR_DUPLICATE_LOCATION:
            return "duplicate location path";
        case ERR_MISSING_VALUE:
            return "missing directive value";
        case ERR_MISSING_SEMICOLON:
            return "missing semicolon";
        case ERR_UNCLOSED_SERVER:
            return "unclosed server block";
        case ERR_UNCLOSED_LOCATION:
            return "unclosed location block";
        case ERR_EXPECTED_ON_OFF:
            return "expected 'on' or 'off'";
        case ERR_INVALID_PATH:
            return "invalid path";
        case ERR_INVALID_METHOD:
            return "unsupported HTTP method";
        case ERR_DUPLICATE_VALUE:
            return "duplicate value";
        case ERR_INVALID_SIZE:
            return "invalid size value";
        case ERR_INVALID_EXTENSION:
            return "invalid file extension";
        case ERR_INVALID_SERVER_NAME:
            return "invalid server name";
        case ERR_INVALID_STATUS_CODE:
            return "invalid HTTP status code";
        case ERR_INVALID_LISTEN:
            return "invalid listen address";
        case ERR_INVALID_HOST:
            return "invalid host";
        case ERR_INVALID_PORT:
            return "port must be a number between 1 and 65535";
        case ERR_INVALID_REDIRECT_CODE:
            return "redirect status code must be 301 or 302";
        case ERR_INVALID_REDIRECT_TARGET:
            return "redirect target must be an absolute path or HTTP URL";
        case ERR_INVALID_UPLOAD_CONFIG:
            return "upload and upload_path must be configured together";
        case ERR_INVALID_CGI_CONFIG:
            return "cgi_path and cgi_ext must be configured together";
    }
    return "invalid configuration";
}

void throwConfigError(const TokenStream &tokens, int errorCode)
{
    std::ostringstream message;
    message << "Config error";

    if (tokens.atEnd())
    {
        message << " at end of file: " << getErrorMessage(errorCode);
    }
    else
    {
        const Token &token = tokens.peekCurrent();

        message << " at line " << token.line  << ", column " << token.column << ": " << getErrorMessage(errorCode);

        if (!token.text.empty())
            message << " near '" << token.text << "'";
    }

    throw std::runtime_error(message.str());
}

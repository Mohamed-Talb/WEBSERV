#include "./Tokenize/tokenStream.hpp"
#include "../Errors.hpp"
#include <sstream>

std::string getErrorMessage(int errorCode)
{
    switch (errorCode)
    {
        case ERR_EXPECTED_SERVER:
            return "expected 'server'";
        case ERR_EXPECTED_SERVER_BLOCK:
            return "expected a server block";
        case ERR_UNKNOWN_SERVER_DIRECTIVE:
            return "unknown server directive";
        case ERR_UNKNOWN_LOCATION_DIRECTIVE:
            return "unknown location directive";
        case ERR_DUPLICATE_LISTEN:
            return "only one listen directive is allowed per server block";
        case ERR_DUPLICATE_DIRECTIVE:
            return "duplicate directive";
        case ERR_INVALID_LISTEN:
            return "invalid listen address";
        case ERR_INVALID_HOST:
            return "invalid host";
        case ERR_INVALID_PORT:
            return "port must be a number between 1 and 65535";
        case ERR_INVALID_AUTOINDEX:
            return "autoindex must be 'on' or 'off'";
        case ERR_INVALID_UPLOAD:
            return "upload must be 'on' or 'off'";
        case ERR_INVALID_REDIRECT_CODE:
            return "redirect status code must be 301 or 302";
        case ERR_MISSING_SEMICOLON:
            return "missing semicolon";
        case ERR_MISSING_VALUE:
            return "missing directive value";
        case ERR_UNCLOSED_SERVER:
            return "unclosed server block";
        case ERR_UNCLOSED_LOCATION:
            return "unclosed location block";
        case ERR_INVALID_PATH:
            return "invalid path";
        case ERR_INVALID_METHOD:
            return "unsupported HTTP method";
        case ERR_DUPLICATE_METHOD:
            return "duplicate HTTP method";
        case ERR_EMPTY_CONFIG:
            return "configuration file is empty";
        case ERR_INVALID_UPLOAD_CONFIG:
            return "upload and upload_path must be configured together";
        case ERR_INVALID_CGI_CONFIG:
            return "cgi_ext and cgi_path must be configured together";
        case ERR_INVALID_SERVER_NAME:
            return "invalid server name";
        case ERR_INVALID_BODY_SIZE:
            return "client_max_body_size must be greater than zero";
        case ERR_INVALID_ERROR_CODE:
            return "invalid HTTP error status code";
        case ERR_DUPLICATE_ERROR_CODE:
            return "duplicate error status code";
        case ERR_DUPLICATE_LOCATION:
            return "duplicate location path";
        case ERR_INVALID_CGI_EXTENSION:
            return "invalid CGI extension";
        case ERR_INVALID_REDIRECT_TARGET:
            return "invalid redirect target";
    }
    return "invalid configuration";
}


void throwConfigError(const TokenStream &tokens, int errorCode)
{
    std::ostringstream message;
    message << "Config error";

    if (tokens.atEnd())
    {
        message << " at end of file: "
                << getErrorMessage(errorCode);
    }
    else
    {
        const Token &token = tokens.peek();

        message << " at line "
                << token.line
                << ", column "
                << token.column
                << ": "
                << getErrorMessage(errorCode);

        if (!token.text.empty())
            message << " near '" << token.text << "'";
    }

    throw std::runtime_error(message.str());
}

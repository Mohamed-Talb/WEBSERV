#ifndef ERRORS_HPP
#define ERRORS_HPP

#include <string>

#define ERR_EMPTY_CONFIG             1
#define ERR_UNKNOWN_DIRECTIVE        2
#define ERR_DUPLICATE_DIRECTIVE      3
#define ERR_DUPLICATE_LISTEN         4
#define ERR_DUPLICATE_LOCATION       5
#define ERR_MISSING_VALUE            6
#define ERR_MISSING_SEMICOLON        7
#define ERR_UNCLOSED_SERVER          8
#define ERR_UNCLOSED_LOCATION        9

#define ERR_EXPECTED_ON_OFF          10
#define ERR_INVALID_PATH             11
#define ERR_INVALID_METHOD           12
#define ERR_DUPLICATE_VALUE          13
#define ERR_INVALID_SIZE             14
#define ERR_INVALID_EXTENSION        15
#define ERR_INVALID_SERVER_NAME      16
#define ERR_INVALID_STATUS_CODE      17

#define ERR_INVALID_LISTEN           18
#define ERR_INVALID_HOST             19
#define ERR_INVALID_PORT             20
#define ERR_INVALID_REDIRECT_CODE    21
#define ERR_INVALID_REDIRECT_TARGET  22

#define ERR_INVALID_UPLOAD_CONFIG    23
#define ERR_INVALID_CGI_CONFIG       24

class TokenStream;

std::string getErrorMessage(int errorCode);
void throwConfigError(const TokenStream &tokens, int errorCode);

#endif
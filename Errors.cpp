#include "Errors.hpp"
#include <string.h>    // strerror
ServerException::ServerException(const std::string& context, const std::string& msg) : std::runtime_error("[" + context + "] " + msg + ": " + strerror(errno)) {}

void logError(const std::string& context, const std::string& msg)
{
    std::cerr << "[ERROR][" << context << "] " << msg << ": " << strerror(errno) << "\n";
}

// void throwError(ConfigErrorType type, const std::string &target)
// {
//     std::string message = "[Config Error] ";
//     switch (type)
//     {
//         case ERR_DUPLICATE_DIRECTIVE:
//             message += "Duplicate directive: '" + target + "' can only appear once per block.";
//             break;
//         case ERR_DUPLICATE_VALUE:
//             message = "Duplicate value provided: '" + target + "' is already listed.";
//             break;
//         case ERR_MISSING_VALUE:
//             message += "Expected a value for the '" + target + "' directive.";
//             break;
//         case ERR_INVALID_VALUE:
//             message += "Invalid value provided: '" + target + "'.";
//             break;
//         case ERR_MISSING_SEMICOLON:
//             message += "Missing ';' after the '" + target + "' directive.";
//             break;
//         case ERR_INVALID_SYNTAX:
//             message += "Invalid syntax near '" + target + "'.";
//             break;
//         default:
//             message += "Unknown error near '" + target + "'.";
//             break;
//     }
//     throw std::runtime_error(message);
// }
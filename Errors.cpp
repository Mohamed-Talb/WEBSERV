#include "Errors.hpp"
#include <string.h>    // strerror
ServerException::ServerException(const std::string& context, const std::string& msg) : std::runtime_error("[" + context + "] " + msg + ": " + strerror(errno)) {}

void logError(const std::string& context, const std::string& msg)
{
    std::cerr << "[ERROR][" << context << "] " << msg << ": " << strerror(errno) << "\n";
}

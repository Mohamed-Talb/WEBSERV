#ifndef VALUESPARSER_HPP
#define VALUESPARSER_HPP

#include <string>
#include <vector>
#include <map>
#include <stdexcept>
#include <cstdlib>
#include <sys/types.h>
#include "../Helpers.hpp"
#include "config.hpp"
#include <algorithm>
#include <limits>  
#include <sstream>
#include "tokenStream.hpp"

namespace valuesParser
{
    int parsePortValue(TokenStream &tokens);
    size_t parseBodySizeValue(TokenStream &tokens);
    std::string parseCgiExtValue(TokenStream &tokens);
    std::string parseCgiPathValue(TokenStream &tokens);
    std::string parseLocationPath(TokenStream &tokens);
    std::string parseFilesystemPath(TokenStream &tokens);
    std::string parseErrorPagePathValue(TokenStream &tokens);
    std::string parseRedirectTargetValue(TokenStream &tokens);
    std::vector<std::string> parseIndexesList(TokenStream &tokens);
    std::vector<std::string> parseWordListUntilSemicolon(TokenStream &tokens, const std::string &directiveName);
}

#endif
#ifndef VALUESPARSER_HPP
#define VALUESPARSER_HPP

#include <map>
#include <string>
#include <vector>
#include <limits>  
#include <sstream>
#include <cstdlib>
#include <algorithm>
#include <stdexcept>
#include <sys/types.h>
#include "../Helpers.hpp"
#include "./Tokenize/tokenStream.hpp"

namespace valuesParser
{
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
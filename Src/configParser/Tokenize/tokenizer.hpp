#ifndef TOKENIZER_HPP
#define TOKENIZER_HPP

#include <string>
#include <vector>
#include "token.hpp"

class Tokenizer
{
    private:
        static bool isSeparator(char character);
    public:
        static std::vector<Token> tokenize(const std::string &filePath);
};

#endif
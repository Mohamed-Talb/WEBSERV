#ifndef TOKEN_HPP
#define TOKEN_HPP

#include <string>
#include <cstddef>

struct Token
{
    size_t line;
    size_t column;
    std::string text;

    Token() : text(""), line(0), column(0) {}
    Token(const std::string &tokenText, size_t tokenLine, size_t tokenColumn) 
        : text(tokenText),line(tokenLine),column(tokenColumn){}
};

#endif
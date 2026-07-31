#ifndef TOKEN_STREAM_HPP
#define TOKEN_STREAM_HPP

#include <string>
#include <vector>
#include "token.hpp"

class TokenStream
{
    private:
    size_t position;
    std::vector<Token> tokens;

    bool isSpecialToken(const std::string &text) const;
    void throwUnexpected(const std::string &expected) const;

    public:
    TokenStream();
    TokenStream(const std::string &filePath);

    Token consume();
    bool atEnd() const;
    const Token &previous() const;
    const Token &peekCurrent() const;
    Token expect(const std::string &expected);
    Token expectValue(const std::string &description);
    const Token &peekValue(const std::string &description) const;
};

#endif
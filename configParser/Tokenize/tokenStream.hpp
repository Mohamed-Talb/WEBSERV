#ifndef TOKEN_STREAM_HPP
#define TOKEN_STREAM_HPP

#include <string>
#include <vector>
#include "token.hpp"

class TokenStream
{
    private:
        std::vector<Token> tokens;
        size_t position;

        bool isSpecialToken(const std::string &text) const;
        void throwUnexpected(const std::string &expected) const;

    public:
        TokenStream();

        explicit TokenStream(const std::string &filePath);

        bool atEnd() const;

        const Token &peek() const;
        const Token &previous() const;

        Token consume();

        Token expect(const std::string &expected);

        Token expectValue(const std::string &description);
        void expectSemicolon();
};

#endif
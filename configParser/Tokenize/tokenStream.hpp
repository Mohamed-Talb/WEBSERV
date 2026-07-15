#ifndef TOKENSTREAM_HPP
#define TOKENSTREAM_HPP

#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include "token.hpp"

class TokenStream
{
    private:
        std::vector<Token> _tokens;
        size_t _position;

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
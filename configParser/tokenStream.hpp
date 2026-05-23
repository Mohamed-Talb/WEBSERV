#ifndef TOKENSTREAM_HPP
#define TOKENSTREAM_HPP

#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>

class TokenStream
{
    private:
    std::vector<std::string> tokens;
    std::vector<std::string>::const_iterator it;

    public:
    TokenStream(std::string tokens);
    TokenStream();

    bool hasMore() const;
    const std::string &current() const;
    std::string expect(const std::string &err);
    TokenStream &operator=(const TokenStream &other);
    void expectSemicolon(const std::string &directive);
};

#endif
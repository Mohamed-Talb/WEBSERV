#ifndef TOKENSTREAM_HPP
#define TOKENSTREAM_HPP

#include <vector>
#include <string>
#include <stdexcept>

class TokenStream
{
    private:
    const std::vector<std::string> &tokens;
    std::vector<std::string>::const_iterator it;

    public:
    TokenStream(const std::vector<std::string> &tokens) : tokens(tokens), it(tokens.begin())
    {}

    bool hasMore() const
    {
        return it != tokens.end();
    }

    const std::string &current() const
    {
        if (it == tokens.end())
            throw std::runtime_error("Unexpected end of config");

        return *it;
    }

    std::string expect(const std::string &err)
    {
        if (it == tokens.end())
            throw std::runtime_error(err);

        std::string value = *it;
        ++it;
        return value;
    }

    void expectSemicolon(const std::string &directive)
    {
        if (it == tokens.end() || *it != ";")
            throw std::runtime_error("Missing ';' after " + directive);
        ++it;
    }
};

#endif
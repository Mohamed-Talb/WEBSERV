#include "tokenStream.hpp"
#include "tokenizer.hpp"

#include <sstream>
#include <stdexcept>

TokenStream::TokenStream() : position(0) {}

TokenStream::TokenStream(const std::string &filePath): tokens(Tokenizer::tokenize(filePath)), position(0) {}

bool TokenStream::atEnd() const
{
    return position >= tokens.size();
}

const Token &TokenStream::peek() const
{
    if (atEnd())
        throw std::runtime_error("Unexpected end of configuration");
    return tokens[position];
}

const Token &TokenStream::peekValue(const std::string &description) const
{
    if (atEnd())
        throwUnexpected(description);

    const Token &token = peek();

    if (isSpecialToken(token.text))
        throwUnexpected(description);

    return token;
}

const Token &TokenStream::previous() const
{
    if (position == 0)
    {
        throw std::runtime_error("No previous token");
    }
    return tokens[position - 1];
}

Token TokenStream::consume()
{
    if (atEnd())
    {
        throw std::runtime_error("Unexpected end of configuration");
    }
    Token token = tokens[position];
    ++position;
    return token;
}

bool TokenStream::isSpecialToken(const std::string &text) const
{
    return (text == "{" || text == "}" || text == ";");
}

void TokenStream::throwUnexpected(const std::string &expected) const
{
    std::ostringstream message;

    message << "Config error";
    if (atEnd())
    {
        message << ": expected " << expected << ", but reached end of file";
        throw std::runtime_error(message.str());
    }

    const Token &token = peek();
    message << " at line " << token.line << ", column " << token.column << ": expected " << expected << ", received '" << token.text << "'";
    throw std::runtime_error(message.str());
}

Token TokenStream::expect(const std::string &expected)
{
    if (atEnd() ||peek().text != expected)
    {
        throwUnexpected("'" + expected + "'");
    }
    return consume();
}

Token TokenStream::expectValue(const std::string &description)
{
    if (atEnd())
        throwUnexpected(description);

    const Token &token = peek();
    if (isSpecialToken(token.text))
        throwUnexpected(description);

    return consume();
}

void TokenStream::expectSemicolon()
{
    expect(";");
}
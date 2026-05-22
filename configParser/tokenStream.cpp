#include "tokenStream.hpp"

std::vector<std::string> fileToTokens(const std::string &filepath)
{
    std::vector<std::string> tokens;
    std::ifstream file(filepath.c_str());
    std::string specials = "{;}";

    if (!file.is_open())
        throw std::runtime_error("Could not open config file: " + filepath);

    std::string line;
    while (std::getline(file, line))
    {
        size_t commentPos = line.find('#');

        if (commentPos != std::string::npos)
            line.erase(commentPos);

        std::string processedLine;

        for (size_t i = 0; i < line.size(); ++i)
        {
            if (specials.find(line[i]) != std::string::npos)
            {
                processedLine += ' ';
                processedLine += line[i];
                processedLine += ' ';
            }
            else
            {
                processedLine += line[i];
            }
        }
        std::stringstream ss(processedLine);
        std::string token;

        while (ss >> token)
            tokens.push_back(token);
    }
    return tokens;
}


TokenStream::TokenStream(std::string filePath)
{
    tokens = fileToTokens(filePath);
    it = tokens.begin();
}

TokenStream::TokenStream() {}

TokenStream &TokenStream::operator=(const TokenStream &other)
{
    if (this != &other) 
    {
        size_t offset = std::distance(other.tokens.begin(), other.it);
        this->tokens = other.tokens;
        this->it = this->tokens.begin() + offset;
    }
    return *this;
}

bool TokenStream::hasMore() const
{
    return it != tokens.end();
}

const std::string &TokenStream::current() const
{
    if (it == tokens.end())
        throw std::runtime_error("Unexpected end of config");

    return *it;
}

std::string TokenStream::expect(const std::string &err)
{
    if (it == tokens.end())
        throw std::runtime_error(err);

    std::string value = *it;
    ++it;
    return value;
}

void TokenStream::expectSemicolon(const std::string &directive)
{
    if (it == tokens.end() || *it != ";")
        throw std::runtime_error("Missing ';' after " + directive);
    ++it;
}
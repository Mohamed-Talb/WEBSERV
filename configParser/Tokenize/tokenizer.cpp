#include "Tokenizer.hpp"

#include <fstream>
#include <stdexcept>
#include <cctype>

bool Tokenizer::isSeparator(char character)
{
    return (
        character == '{' ||
        character == '}' ||
        character == ';'
    );
}

std::vector<Token> Tokenizer::tokenize(
    const std::string &filePath
)
{
    std::ifstream file(filePath.c_str());

    if (!file.is_open())
    {
        throw std::runtime_error(
            "Could not open config file: " + filePath
        );
    }

    std::vector<Token> tokens;
    std::string line;
    size_t lineNumber = 0;

    while (std::getline(file, line))
    {
        ++lineNumber;

        size_t position = 0;

        while (position < line.size())
        {
            char current = line[position];

            // Ignore the rest of the line after '#'.
            if (current == '#')
                break;

            // Ignore spaces and tabs.
            if (std::isspace(
                    static_cast<unsigned char>(current)
                ))
            {
                ++position;
                continue;
            }

            // Add "{", "}" or ";" as an individual token.
            if (isSeparator(current))
            {
                tokens.push_back(
                    Token(
                        std::string(1, current),
                        lineNumber,
                        position + 1
                    )
                );

                ++position;
                continue;
            }

            // Read an ordinary word or value.
            size_t start = position;

            while (position < line.size())
            {
                current = line[position];

                if (
                    current == '#' ||
                    isSeparator(current) ||
                    std::isspace(
                        static_cast<unsigned char>(current)
                    )
                )
                {
                    break;
                }

                ++position;
            }

            tokens.push_back(
                Token(
                    line.substr(start, position - start),
                    lineNumber,
                    start + 1
                )
            );
            if (
                position < line.size() &&
                line[position] == '#'
            )
            {
                break;
            }
        }
    }

    return tokens;
}
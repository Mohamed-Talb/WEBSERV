#include "configParser.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>

std::vector<std::string> tokenize(const std::string &filepath)
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
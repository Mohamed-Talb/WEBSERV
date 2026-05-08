#include <vector>
#include <sstream>

std::string normalizePath(const std::string &path)
{
    std::vector<std::string> stack;
    std::istringstream iss(path);
    std::string token;

    while (std::getline(iss, token, '/'))
    {
        if (token.empty() || token == ".")
        {
            continue;
        }
        else if (token == "..")
        {
            if (!stack.empty())
                stack.pop_back();
        }
        else
        {
            stack.push_back(token); 
        }
    }
    std::string normalized = "";
    for (size_t i = 0; i < stack.size(); ++i)
    {
        normalized += "/" + stack[i];
    }
    return normalized.empty() ? "/" : normalized;
}
#include <vector>
#include <sstream>

bool normalizePath(const std::string &path, std::string &normalized)
{
    std::vector<std::string> stack;
    std::istringstream iss(path);
    std::string token;

    while (std::getline(iss, token, '/'))
    {
        if (token.empty() || token == ".")
            continue;

        if (token == "..")
        {
            if (stack.empty())
                return false;

            stack.pop_back();
            continue;
        }

        stack.push_back(token);
    }

    normalized.clear();

    for (size_t i = 0; i < stack.size(); ++i)
        normalized += "/" + stack[i];

    if (normalized.empty())
        normalized = "/";

    return true;
}
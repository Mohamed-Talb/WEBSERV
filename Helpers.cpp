#include "Helpers.hpp"

std::string intToString(int value)
{
    std::stringstream ss;
    ss << value;
    return ss.str();
}

std::string toUpper(std::string value)
{
    for (size_t i = 0; i < value.size(); ++i)
        value[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(value[i])));
    return value;
}

std::string toLower(std::string value)
{
    for (size_t i = 0; i < value.size(); ++i)
        value[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(value[i])));
    return value;
}

std::string trim(const std::string &value)
{
    size_t start = 0;
    while (start < value.size() && (value[start] == ' ' || value[start] == '\t'))
        ++start;
    size_t end = value.size();
    while (end > start && (value[end - 1] == ' ' || value[end - 1] == '\t'))
        --end;
    return value.substr(start, end - start);
}

ssize_t myStold(const std::string &str) 
{
	if (str[0] == '-')
		return -1;
    std::stringstream ss(str);
    unsigned long value;
    ss >> value;
    if (ss.fail())
    	return -1;
    return value;
}


std::string joinPath(std::string left, std::string right)
{
    if (left.empty())
        return right;

    while (left.size() > 1 && left[left.size() - 1] == '/')
        left.erase(left.size() - 1);

    while (!right.empty() && right[0] == '/')
        right.erase(0, 1);

    if (right.empty())
        return left;

    return left + "/" + right;
}


std::string mergeSlashes(const std::string &path)
{
    std::string result;
    bool lastSlash = false;

    for (size_t i = 0; i < path.size(); ++i)
    {
        if (path[i] == '/')
        {
            if (!lastSlash)
                result += '/';
            lastSlash = true;
        }
        else
        {
            result += path[i];
            lastSlash = false;
        }
    }
    return result;
}


bool isOnlyDigits(const std::string &s)
{
    if (s.empty())
        return false;
    for (size_t i = 0; i < s.size(); ++i)
    {
        if (!std::isdigit(static_cast<unsigned char>(s[i])))
            return false;
    }
    return true;
}
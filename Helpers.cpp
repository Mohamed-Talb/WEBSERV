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
    if (left == "/")
        return "/" + right;
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


bool isValidHost(const std::string &host)
{
    if (host == "localhost")
        return true;

    if (host.empty())
        return false;

    int dots = 0;
    std::string part;

    for (size_t i = 0; i <= host.size(); ++i)
    {
        if (i == host.size() || host[i] == '.')
        {
            if (part.empty())
                return false;
            if (part.size() > 3)
                return false;
            for (size_t j = 0; j < part.size(); ++j)
            {
                if (!std::isdigit(part[j]))
                    return false;
            }
            int value = std::atoi(part.c_str());
            if (value < 0 || value > 255)
                return false;

            ++dots;
            part.clear();
        }
        else
        {
            part += host[i];
        }
    }
    return dots == 4;
}

bool isValidServerName(const std::string &name)
{
    if (name.empty() || name.length() > 253)
        return false;

    if (name == "_")
        return true;

    if (name[0] == '-' || name[0] == '.' || name[name.length() - 1] == '-' || name[name.length() - 1] == '.')
        return false;

    size_t labelLength = 0;

    for (size_t i = 0; i < name.length(); ++i)
    {
        char c = name[i];

        if (std::isalnum(c) || c == '-')
        {
            labelLength++;
            if (labelLength > 63)
                return false;
        }
        else if (c == '.')
        {
            if (i > 0 && name[i - 1] == '.')
                return false; 
            labelLength = 0; 
        }
        else
        {
            return false;
        }
    }
    return true;
}


bool isValidErrorCode(int code)
{
    switch (code)
    {
        case 400: // Bad Request
        case 403: // Forbidden
        case 404: // Not Found
        case 405: // Method Not Allowed
        case 408: // Request Timeout
        case 413: // Payload Too Large
        case 414:
        case 431:
        case 500: // Internal Server Error
        case 501: // Not Implemented
        case 502: // Bad Gateway
        case 503: // Service Unavailable
        case 504: // Gateway Timeout
        case 505: // HTTP Version Not Supported
            return true;
        default:
            return false;
    }
}


int hexDigit(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';

    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;

    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;

    return -1;
}
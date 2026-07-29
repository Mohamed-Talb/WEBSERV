#include "./HttpUtils.hpp"


std::vector<std::string> splitHeaderValues(const std::string &value)
{
    std::vector<std::string> values;
    size_t start = 0;

    while (start <= value.size())
    {
        size_t comma = value.find(',', start);
        std::string item;
        if (comma == std::string::npos)
            item = value.substr(start);
        else
            item = value.substr(start, comma - start);
        item = trim(item);
        if (!item.empty())
            values.push_back(item);
        if (comma == std::string::npos)
            break;
        start = comma + 1;
    }
    return values;
}


bool isCommaSeparatedHeader(const std::string &name)
{
    return name == "connection" || name == "transfer-encoding" || name == "te"
        || name == "upgrade"
        || name == "accept"
        || name == "accept-encoding"
        || name == "accept-language"
        || name == "cookie"
        || name == "cache-control";
}



bool urlDecode(const std::string &input, std::string &output)
{
    output.clear();
    output.reserve(input.size());

    for (size_t i = 0; i < input.size(); ++i)
    {
        if (input[i] != '%')
        {
            unsigned char current = static_cast<unsigned char>(input[i]);
            if (current == 0 || current < 0x20 || current == 0x7F)
            {
                return false;
            }
            output += input[i];
            continue;
        }

        if (i + 2 >= input.size())
            return false;

        unsigned char first =  static_cast<unsigned char>(input[i + 1]);
        unsigned char second = static_cast<unsigned char>(input[i + 2]);

        if (!std::isxdigit(first) || !std::isxdigit(second))
        {
            return false;
        }
        int high = hexDigit(input[i + 1]);
        int low = hexDigit(input[i + 2]);
        if (high < 0 || low < 0)
            return false;
        int decodedValue = high * 16 + low;
        if (decodedValue == 0 || decodedValue < 0x20 || decodedValue == 0x7F)
        {
            return false;
        }
        output += static_cast<char>(decodedValue);
        i += 2;
    }
    return true;
}

bool parseDecimalSize(const std::string &value, size_t &result)
{
    if (value.empty())
        return false;

    for (size_t i = 0; i < value.size(); ++i)
    {
        unsigned char character =
            static_cast<unsigned char>(value[i]);

        if (!std::isdigit(character))
            return false;
    }

    std::istringstream stream(value);
    size_t parsedValue = 0;

    stream >> parsedValue;

    if (stream.fail() || !stream.eof())
        return false;

    result = parsedValue;
    return true;
}

bool parseHexSize(const std::string &value, size_t &result)
{
    if (value.empty())
        return false;

    for (size_t i = 0; i < value.size(); ++i)
    {
        unsigned char character =
            static_cast<unsigned char>(value[i]);

        if (!std::isxdigit(character))
            return false;
    }

    std::istringstream stream(value);
    size_t parsedValue = 0;

    stream >> std::hex >> parsedValue;

    if (stream.fail() || !stream.eof())
        return false;

    result = parsedValue;
    return true;
}
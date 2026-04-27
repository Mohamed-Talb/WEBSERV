#include "configParser.hpp"

std::string expect(TokenIt &it, const Tokens &tokens, const std::string &err)
{
    if (it == tokens.end())
        throw std::runtime_error(err);
    return *it++;
}

void expectSemicolon(TokenIt &it, const Tokens &tokens, const std::string &directive)
{
    if (expect(it, tokens, "Missing ';' after " + directive) != ";")
        throw std::runtime_error("Expected ';' after " + directive);
}

int parsePort(TokenIt &it, const Tokens &tokens)
{
    std::string value = expect(it, tokens, "Missing port");
    unsigned long port;
    try
    {
        port = myStoul(value);
    }
    catch (...)
    {
        throw std::runtime_error("Invalid port: " + value);
    }
    if (port > 65535)
        throw std::runtime_error("Invalid port: " + value);
    return static_cast<int>(port);
}

size_t parseBodySize(TokenIt &it, const Tokens &tokens)
{
    std::string value = expect(it, tokens, "Missing body size value");
    try
    {
        return myStoul(value);
    }
    catch (...)
    {
        throw std::runtime_error("Invalid client_max_body_size value: " + value);
    }
}

std::string parseRoot(TokenIt &it, const Tokens &tokens)
{
    std::string root = expect(it, tokens, "Missing root");

    while (root.size() > 1 && root[root.size() - 1] == '/')
        root.erase(root.size() - 1);

    if (root.empty())
        throw std::runtime_error("Invalid root path");

    return root;
}

std::string parseLocationPath(TokenIt &it, const Tokens &tokens)
{
    std::string path = expect(it, tokens, "Missing location path");

    if (path.empty() || path[0] != '/')
        throw std::runtime_error("Location path must start with '/'");

    while (path.size() > 1 && path[path.size() - 1] == '/')
        path.erase(path.size() - 1);

    return path;
}

std::string parseIndex(TokenIt &it, const Tokens &tokens)
{
    std::string index = expect(it, tokens, "Missing index value");

    while (!index.empty() && index[0] == '/')
        index.erase(0, 1);

    if (index.empty())
        throw std::runtime_error("Index cannot be empty");

    if (index.find("..") != std::string::npos)
        throw std::runtime_error("Invalid index path: directory traversal");

    return index;
}

std::string parseCgiExt(TokenIt &it, const Tokens &tokens)
{
    std::string ext = expect(it, tokens, "Missing cgi_ext");

    if (ext.empty())
        throw std::runtime_error("Invalid cgi_ext");

    if (ext[0] != '.')
        ext = "." + ext;

    return ext;
}

std::string parseErrorPagePath(const std::string& raw)
{
    if (raw.empty())
        throw std::runtime_error("Invalid error_page path");

    if (raw.find("..") != std::string::npos)
        throw std::runtime_error("Invalid error_page path");

    if (raw[0] != '/')
        return "/" + raw;

    return raw;
}
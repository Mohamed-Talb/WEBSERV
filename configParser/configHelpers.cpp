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
	if (value.size() > 5)
        throw std::runtime_error("Invalid port: " + value);
    int port;
    try
    {
        port = std::atoi(value.c_str());
    }
    catch (...)
    {
        throw std::runtime_error("Invalid port: " + value);
    }
    if (0 < port || port > 65535)
        throw std::runtime_error("Invalid port: " + value);
    return static_cast<int>(port);
}

size_t parseBodySize(TokenIt &it, const Tokens &tokens)
{
    std::string value = expect(it, tokens, "Missing body size value");
    try
    {
        return myStold(value);
    }
    catch (...)
    {
        throw std::runtime_error("Invalid client_max_body_size value: " + value);
    }
}

std::string parseRoot(TokenIt &it, const Tokens &tokens)
{
    std::string root = expect(it, tokens, "Missing root");

    root = mergeSlashes(root);

    while (root.size() > 1 && root[root.size() - 1] == '/')
        root.erase(root.size() - 1);

    if (root.empty())
        throw std::runtime_error("Invalid root path");

    if (root.find("..") != std::string::npos)
        throw std::runtime_error("Invalid root path: directory traversal");

    return root;
}

std::string parseLocationPath(TokenIt &it, const Tokens &tokens)
{
    std::string path = expect(it, tokens, "Missing location path");

    path = mergeSlashes(path);

    if (path.empty() || path[0] != '/')
        throw std::runtime_error("Location path must start with '/'");

    while (path.size() > 1 && path[path.size() - 1] == '/')
        path.erase(path.size() - 1);

    if (path.find("..") != std::string::npos)
        throw std::runtime_error("Invalid location path: directory traversal");

    return path;
}

std::vector<std::string> parseIndexes(TokenIt &it, const Tokens &tokens)
{
    std::vector<std::string> indexes;
    while (it != tokens.end() && *it != ";")
    {
        std::string index = expect(it, tokens, "Missing index value");

        index = mergeSlashes(index);

        while (!index.empty() && index[0] == '/')
            index.erase(0, 1);

        if (index.empty())
            throw std::runtime_error("Index cannot be empty");
		
        if (index.find("..") != std::string::npos)
            throw std::runtime_error("Invalid index path: directory traversal");

        indexes.push_back(index);
    }
    if (it == tokens.end())
        throw std::runtime_error("Missing ';' after index");

    if (indexes.empty())
        throw std::runtime_error("index directive requires at least one file");
    ++it;
    return indexes;
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
    std::string path = mergeSlashes(raw);

    if (path.empty())
        throw std::runtime_error("Invalid error_page path");

    if (path.find("..") != std::string::npos)
        throw std::runtime_error("Invalid error_page path");

    if (path[0] != '/')
        path = "/" + path;

    return path;
}


// void validateServerConfig(ServerConfig &conf)
// {
//     if (conf.host.empty())
//         conf.host = "127.0.0.1";

//     if (conf.port <= 0 || conf.port > 65535)
//         throw std::runtime_error("Invalid server port");

//     if (conf.root.empty())
//         conf.root = "./www";

//     if (conf.index.empty())
//         conf.index = "index.html";

//     for (size_t i = 0; i < conf.Locations.size(); ++i)
//     {
//         Location &loc = conf.Locations[i];

//         if (loc.root.empty())
//             loc.root = conf.root;

//         if (loc.index.empty())
//             loc.index = conf.index;

//         if (loc.autoindex.empty())
//             loc.autoindex = "off";

//         if (loc.methods.empty())
//         {
//             loc.methods.push_back("GET");
//             loc.methods.push_back("DELETE");
//         }
//     }
// }
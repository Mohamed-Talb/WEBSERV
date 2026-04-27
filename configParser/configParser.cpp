#include "configParser.hpp"

/*
===============================================================================
 PATH HANDLING RULES (CONFIG PARSER + HTTP RESOLUTION)
===============================================================================

We follow Postel’s Law:
→ Accept flexible input
→ Normalize internally for consistency and safety

------------------------------------------------------------------------------
| ELEMENT          | ACCEPTED INPUT              | STORED FORMAT              |
------------------------------------------------------------------------------
| root             | "./www", "./www/"           | "./www"                    |
| location.path    | "/", "/img", "/img/"        | "/", "/img"                |
| index            | "index.html", "/index.html" | "index.html"               |
| cgi_path         | "./cgi-bin", "./cgi-bin/"   | "./cgi-bin"                |
| cgi_ext          | ".py", "py"                 | ".py"                      |
| error_page path  | "404.html", "/404.html"     | "/404.html"                |
------------------------------------------------------------------------------

------------------------------------------------------------------------------
 NORMALIZATION RULES
------------------------------------------------------------------------------

1. ROOT
   - Remove trailing slashes
   - Example:
        "./www/" → "./www"

2. LOCATION PATH
   - Must start with '/'
   - Remove trailing slash (except "/")
   - Example:
        "/images/" → "/images"

3. INDEX
   - Remove leading slashes
   - Must be relative to root
   - Reject ".." (security)
   - Example:
        "/index.html" → "index.html"

4. CGI EXT
   - Must start with '.'
   - Example:
        "py" → ".py"

5. ERROR PAGE PATH
   - Must start with '/'
   - Reject ".."
   - Example:
        "404.html" → "/404.html"

------------------------------------------------------------------------------
 SECURITY RULES
------------------------------------------------------------------------------

- Reject any path containing ".." (directory traversal)
- Never allow escaping the root directory

Multiple slashes should be collapsed:

    "///img//cat.png" → "/img/cat.png"
*/


std::vector<std::string> prepConf(const std::string& filepath)
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

Location parseLocation(const std::vector<std::string> &tokens, std::vector<std::string>::iterator &it)
{
    Location loc;

    loc.path = parseLocationPath(it, tokens);

    if (expect(it, tokens, "Expected '{'") != "{")
        throw std::runtime_error("Expected '{'");

    while (it != tokens.end())
    {
        if (*it == "}")
        {
            ++it;
            return loc;
        }
        std::string key = *it;

        if (key == "methods")
        {
            ++it;

            while (it != tokens.end() && *it != ";")
                loc.methods.push_back(*it++);

            if (it == tokens.end())
                throw std::runtime_error("Missing ';' after methods");

            ++it;
        }
        else if (key == "root")
        {
            loc.root = parseRoot(++it, tokens);
            expectSemicolon(it, tokens, "root");
        }
        else if (key == "autoindex")
        {
            loc.autoindex = expect(++it, tokens, "Missing autoindex value");

            if (loc.autoindex != "on" && loc.autoindex != "off")
                throw std::runtime_error("autoindex must be 'on' or 'off'");

            expectSemicolon(it, tokens, "autoindex");
        }
        else if (key == "index")
        {
			++it;
			loc.indexes = parseIndexes(it, tokens);
        }
        else if (key == "cgi_path")
        {
            loc.cgiPath = parseRoot(++it, tokens);
            expectSemicolon(it, tokens, "cgi_path");
        }
        else if (key == "cgi_ext")
        {
            loc.cgiExt = parseCgiExt(++it, tokens);
            expectSemicolon(it, tokens, "cgi_ext");
        }
        else
        {
            throw std::runtime_error("Unknown location directive: " + key);
        }
    }
    throw std::runtime_error("Unclosed location block");
}
void parseErrorPage(ServerConfig &conf, std::vector<std::string>::iterator &it, const std::vector<std::string> &tokens)
{
    std::vector<std::string> values;

    for (++it; it != tokens.end() && *it != ";"; ++it)
        values.push_back(*it);

    if (it == tokens.end())
        throw std::runtime_error("Missing ';' after error_page");

    if (values.size() < 2)
        throw std::runtime_error("Invalid error_page syntax: missing codes or path.");

    std::string path = parseErrorPagePath(values.back());

    for (size_t i = 0; i < values.size() - 1; ++i)
    {
        unsigned long code;
        try
        {
            code = myStold(values[i]);
        }
        catch (...)
        {
            throw std::runtime_error("Invalid error code in config: " + values[i]);
        }
        if (code < 300 || code > 599)
            throw std::runtime_error("Invalid error code in config: " + values[i]);
        conf.errorPage[static_cast<int>(code)] = path;
    }
}

struct CompareLocations
{
    bool operator()(const Location &a, const Location &b) const
    {
        return a.path.size() > b.path.size();
    }
};

ServerConfig parseServer(const std::vector<std::string>& tokens, std::vector<std::string>::iterator &it)
{
    ServerConfig conf;

    if (expect(it, tokens, "Expected '{'") != "{")
        throw std::runtime_error("Expected '{'");
    while (it != tokens.end())
    {
        if (*it == "}")
        {
            ++it;
            return conf;
        }
        std::string key = *it;
        if (key == "host")
        {
            conf.host = expect(++it, tokens, "Missing host");
            expectSemicolon(it, tokens, "host");
        }
        else if (key == "listen")
        {
            conf.port = parsePort(++it, tokens);
            expectSemicolon(it, tokens, "listen");
        }
        else if (key == "root")
        {
            conf.root = parseRoot(++it, tokens);
            expectSemicolon(it, tokens, "root");
        }
        else if (key == "server_name")
        {
            ++it;
            while (it != tokens.end() && *it != ";")
                conf.serverName.push_back(*it++);
            if (it == tokens.end())
                throw std::runtime_error("Missing ';' after server_name");
            ++it;
        }
        else if (key == "location")
        {
            ++it;
            conf.Locations.push_back(parseLocation(tokens, it));
        }
        else if (key == "index")
        {
           	++it;
			conf.indexes = parseIndexes(it, tokens);
        }
        else if (key == "error_page")
        {
            parseErrorPage(conf, it, tokens);
			if (it == tokens.end())
    			throw std::runtime_error("Missing ';' after error_page");
            ++it;
        }
        else if (key == "client_max_body_size")
        {
            conf.client_max_body_size = parseBodySize(++it, tokens);
            expectSemicolon(it, tokens, "client_max_body_size");
        }
        else
            throw std::runtime_error("Unknown server directive: " + key);
    }
    throw std::runtime_error("Unclosed server block");
}

std::vector<ServerConfig> parseTokens(std::vector<std::string> tokens)
{
    std::vector<ServerConfig> allServers;
    std::vector<std::string>::iterator it = tokens.begin();

    while (it != tokens.end())
    {
        if (*it != "server")
            throw std::runtime_error("Expected 'server' keyword");
        ++it;
        ServerConfig currServerConfig = parseServer(tokens, it);
        std::sort(
            currServerConfig.Locations.begin(),
            currServerConfig.Locations.end(),
            CompareLocations()
        );
        allServers.push_back(currServerConfig);
    }
    return allServers;
}


std::vector<ServerConfig> parseConfig()
{
    std::vector<std::string> tokens = prepConf("server.conf");
	std::vector<ServerConfig> allServers = parseTokens(tokens); 
    return allServers;
}


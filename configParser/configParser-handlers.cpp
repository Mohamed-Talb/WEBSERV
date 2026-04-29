#include "configParser.hpp"



void ConfigParser::handleHost(ServerConfig &conf)
{
    if (conf.seenDirectives["host"])
        throw std::runtime_error("duplicate host directive");

    conf.seenDirectives["host"] = true;
    tokens.expect("Expected host");
    conf.host = tokens.expect("Missing host");
    tokens.expectSemicolon("host");
}

void ConfigParser::handleListen(ServerConfig &conf)
{
    if (conf.seenDirectives["listen"])
        throw std::runtime_error("duplicate listen directive");

    conf.seenDirectives["listen"] = true;
    tokens.expect("Expected listen");

    std::string value = tokens.expect("Missing port");
    conf.port = parsePortValue(value);

    tokens.expectSemicolon("listen");
}

void ConfigParser::handleRoot(ServerConfig &conf)
{
    if (conf.seenDirectives["root"])
        throw std::runtime_error("duplicate root directive");

    conf.seenDirectives["root"] = true;
    tokens.expect("Expected root");

    conf.root = parseRootPath();

    tokens.expectSemicolon("root");
}

void ConfigParser::handleServerName(ServerConfig &conf)
{
    if (conf.seenDirectives["server_name"])
        throw std::runtime_error("duplicate server_name directive");

    conf.seenDirectives["server_name"] = true;
    tokens.expect("Expected server_name");

    while (tokens.hasMore() && tokens.current() != ";")
        conf.serverName.push_back(tokens.expect("Missing server_name value"));

    if (!tokens.hasMore())
        throw std::runtime_error("Missing ';' after server_name");

    if (conf.serverName.empty())
        throw std::runtime_error("server_name requires at least one name");

    tokens.expectSemicolon("server_name");
}

void ConfigParser::handleLocation(ServerConfig &conf)
{
    tokens.expect("Expected location");

    Location loc;
    parseLocationBlock(loc);
    for (size_t i = 0; i < conf.Locations.size(); ++i)
    {
        if (conf.Locations[i].path == loc.path)
            throw std::runtime_error("Duplicate location: " + loc.path);
    }
    conf.Locations.push_back(loc);
}

void ConfigParser::handleIndex(ServerConfig &conf)
{
    if (conf.seenDirectives["index"])
        throw std::runtime_error("duplicate index directive");

    conf.seenDirectives["index"] = true;    
    tokens.expect("Expected index");

    conf.indexes = parseIndexesList();
}

void ConfigParser::handleErrorPage(ServerConfig &conf)
{
    std::vector<std::string> values;

    tokens.expect("Expected error_page");

    while (tokens.hasMore() && tokens.current() != ";")
        values.push_back(tokens.expect("Missing error_page value"));

    if (!tokens.hasMore())
        throw std::runtime_error("Missing ';' after error_page");

    if (values.size() < 2)
        throw std::runtime_error("Invalid error_page syntax: missing codes or path");

    std::string path = parseErrorPagePathValue(values.back());

    for (size_t i = 0; i < values.size() - 1; ++i)
    {
        if (!isOnlyDigits(values[i]))
            throw std::runtime_error("Invalid error code in config: " + values[i]);

        unsigned long code = static_cast<unsigned long>(std::atol(values[i].c_str()));

        if (code < 300 || code > 599)
            throw std::runtime_error("Invalid error code in config: " + values[i]);

        conf.errorPage[static_cast<int>(code)] = path;
    }

    tokens.expectSemicolon("error_page");
}

void ConfigParser::handleClientMaxBodySize(ServerConfig &conf)
{
    if (conf.seenDirectives["client_max_body_size"])
        throw std::runtime_error("duplicate client_max_body_size directive");

    conf.seenDirectives["client_max_body_size"] = true;
    tokens.expect("Expected client_max_body_size");

    std::string value = tokens.expect("Missing body size value");
    conf.client_max_body_size = parseBodySizeValue(value);

    tokens.expectSemicolon("client_max_body_size");
}




void ConfigParser::handleLocMethods(Location &loc)
{
    if (loc.seenDirectives["methods"])
        throw std::runtime_error("duplicate methods directive");

    loc.seenDirectives["methods"] = true;
    tokens.expect("Expected methods");

    while (tokens.hasMore() && tokens.current() != ";")
        loc.methods.push_back(tokens.expect("Missing method value"));

    if (!tokens.hasMore())
        throw std::runtime_error("Missing ';' after methods");

    if (loc.methods.empty())
        throw std::runtime_error("methods directive requires at least one method");

    tokens.expectSemicolon("methods");
}

void ConfigParser::handleLocRoot(Location &loc)
{
    if (loc.seenDirectives["root"])
        throw std::runtime_error("duplicate root directive in location");

    loc.seenDirectives["root"] = true;
    tokens.expect("Expected root");

    loc.root = parseRootPath();

    tokens.expectSemicolon("root");
}

void ConfigParser::handleLocAutoindex(Location &loc)
{
    if (loc.seenDirectives["autoindex"])
        throw std::runtime_error("duplicate autoindex directive");

    loc.seenDirectives["autoindex"] = true;
    tokens.expect("Expected autoindex");

    loc.autoindex = tokens.expect("Missing autoindex value");

    if (loc.autoindex != "on" && loc.autoindex != "off")
        throw std::runtime_error("autoindex must be 'on' or 'off'");

    tokens.expectSemicolon("autoindex");
}

void ConfigParser::handleLocIndex(Location &loc)
{
    if (loc.seenDirectives["index"])
        throw std::runtime_error("duplicate index directive in location");

    loc.seenDirectives["index"] = true;
    tokens.expect("Expected index");

    loc.indexes = parseIndexesList();
}

void ConfigParser::handleLocCgiPath(Location &loc)
{
    if (loc.seenDirectives["cgi_path"])
        throw std::runtime_error("duplicate cgi_path directive");

    loc.seenDirectives["cgi_path"] = true;
    tokens.expect("Expected cgi_path");

    loc.cgiPath = parseRootPath();

    tokens.expectSemicolon("cgi_path");
}

void ConfigParser::handleLocCgiExt(Location &loc)
{
    if (loc.seenDirectives["cgi_ext"])
        throw std::runtime_error("duplicate cgi_ext directive");

    loc.seenDirectives["cgi_ext"] = true;
    tokens.expect("Expected cgi_ext");

    std::string value = tokens.expect("Missing cgi_ext");
    loc.cgiExt = parseCgiExtValue(value);

    tokens.expectSemicolon("cgi_ext");
}

void ConfigParser::handleLocRedirect(Location &loc)
{
    if (loc.seenDirectives["redirect"])
        throw std::runtime_error("duplicate redirect directive");

    loc.seenDirectives["redirect"] = true;
    if (loc.redirectCode != 0)
        throw std::runtime_error("duplicate redirect directive");

    tokens.expect("Expected redirect");

    std::string codeValue = tokens.expect("Missing redirect code");

    if (!isOnlyDigits(codeValue))
        throw std::runtime_error("Invalid redirect code");

    loc.redirectCode = std::atoi(codeValue.c_str());

    if (loc.redirectCode != 301 && loc.redirectCode != 302)
        throw std::runtime_error("Invalid redirect code");

    loc.redirectTarget = parseRedirectTargetValue(
        tokens.expect("Missing redirect target")
    );

    tokens.expectSemicolon("redirect");
}

void ConfigParser::handleLocUpload(Location &loc)
{
    if (loc.seenDirectives["upload"])
        throw std::runtime_error("duplicate upload directive");

    loc.seenDirectives["upload"] = true;
    if (!loc.uploadEnabled.empty())
        throw std::runtime_error("duplicate upload directive");

    tokens.expect("Expected upload");

    loc.uploadEnabled = tokens.expect("Missing upload value");

    if (loc.uploadEnabled != "on" && loc.uploadEnabled != "off")
        throw std::runtime_error("upload must be 'on' or 'off'");

    tokens.expectSemicolon("upload");
}

void ConfigParser::handleLocUploadPath(Location &loc)
{
    if (loc.seenDirectives["upload_path"])
        throw std::runtime_error("duplicate upload_path directive");

    loc.seenDirectives["upload_path"] = true;
    if (!loc.uploadPath.empty())
        throw std::runtime_error("duplicate upload_path directive");

    tokens.expect("Expected upload_path");

    loc.uploadPath = parseRootPath();

    tokens.expectSemicolon("upload_path");
}




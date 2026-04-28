#include "configParser.hpp"



void ConfigParser::handleHost(ServerConfig &conf)
{
    tokens.expect("Expected host");

    conf.host = tokens.expect("Missing host");

    tokens.expectSemicolon("host");
}

void ConfigParser::handleListen(ServerConfig &conf)
{
    tokens.expect("Expected listen");

    std::string value = tokens.expect("Missing port");
    conf.port = parsePortValue(value);

    tokens.expectSemicolon("listen");
}

void ConfigParser::handleRoot(ServerConfig &conf)
{
    tokens.expect("Expected root");

    conf.root = parseRootPath();

    tokens.expectSemicolon("root");
}

void ConfigParser::handleServerName(ServerConfig &conf)
{
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

    conf.Locations.push_back(loc);
}

void ConfigParser::handleIndex(ServerConfig &conf)
{
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
    tokens.expect("Expected client_max_body_size");

    std::string value = tokens.expect("Missing body size value");
    conf.client_max_body_size = parseBodySizeValue(value);

    tokens.expectSemicolon("client_max_body_size");
}


void ConfigParser::handleLocMethods(Location &loc)
{
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
    tokens.expect("Expected root");

    loc.root = parseRootPath();

    tokens.expectSemicolon("root");
}

void ConfigParser::handleLocAutoindex(Location &loc)
{
    tokens.expect("Expected autoindex");

    loc.autoindex = tokens.expect("Missing autoindex value");

    if (loc.autoindex != "on" && loc.autoindex != "off")
        throw std::runtime_error("autoindex must be 'on' or 'off'");

    tokens.expectSemicolon("autoindex");
}

void ConfigParser::handleLocIndex(Location &loc)
{
    tokens.expect("Expected index");

    loc.indexes = parseIndexesList();
}

void ConfigParser::handleLocCgiPath(Location &loc)
{
    tokens.expect("Expected cgi_path");

    loc.cgiPath = parseRootPath();

    tokens.expectSemicolon("cgi_path");
}

void ConfigParser::handleLocCgiExt(Location &loc)
{
    tokens.expect("Expected cgi_ext");

    std::string value = tokens.expect("Missing cgi_ext");
    loc.cgiExt = parseCgiExtValue(value);

    tokens.expectSemicolon("cgi_ext");
}

void ConfigParser::handleLocRedirect(Location &loc)
{
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
    if (!loc.uploadPath.empty())
        throw std::runtime_error("duplicate upload_path directive");

    tokens.expect("Expected upload_path");

    loc.uploadPath = parseRootPath();

    tokens.expectSemicolon("upload_path");
}




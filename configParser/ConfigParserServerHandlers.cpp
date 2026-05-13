#include "configParser.hpp"

void ConfigParser::handleHost(ServerConfig &conf)
{
    if (conf.seenDirectives["host"])
        throw std::runtime_error("duplicate host directive");
    conf.seenDirectives["host"] = true;
    tokens.expect("Expected host");
    conf.host = tokens.expect("Missing host");
    if (!isValidHost(conf.host))
        throw std::runtime_error("Invalid host: " + conf.host);
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

    conf.root = parseFilesystemPath();

    tokens.expectSemicolon("root");
}

void ConfigParser::handleServerName(ServerConfig &conf)
{
    if (conf.seenDirectives["server_name"])
        throw std::runtime_error("duplicate server_name directive");

    conf.seenDirectives["server_name"] = true;

    tokens.expect("Expected server_name");

    conf.serverName = parseWordListUntilSemicolon("server_name");
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
    {
        values.push_back(tokens.expect("Missing error_page value"));
    }

    if (!tokens.hasMore())
        throw std::runtime_error("Missing ';' after error_page directive");
    if (values.size() < 2)
        throw std::runtime_error("Invalid error_page syntax: requires at least one status code and a target path");

    size_t pathIndex = values.size() - 1;
    std::string path = parseErrorPagePathValue(values[pathIndex]);

    for (size_t i = 0; i < pathIndex; ++i)
    {
        if (!isOnlyDigits(values[i]))
            throw std::runtime_error("Invalid error_page code syntax (non-numeric): " + values[i]);
        int errorCode = 0;
        std::istringstream stream(values[i]);
        stream >> errorCode;

        if (stream.fail())
            throw std::runtime_error("Invalid error_page status code (overflow): " + values[i]);

        if (!isValidErrorCode(errorCode))
            throw std::runtime_error("Unsupported or invalid HTTP error status code: " + values[i]);

        if (conf.errorPage.count(errorCode))
            throw std::runtime_error("Duplicate error_page declaration for status code: " + values[i]);

        conf.errorPage[errorCode] = path;
    }

    tokens.expectSemicolon("error_page");
}


size_t ConfigParser::parseBodySizeValue(const std::string &value)
{
    if (value.empty())
        throw std::runtime_error("Empty client_max_body_size directive value");

    size_t i = 0;
    while (i < value.size() && std::isdigit(static_cast<unsigned char>(value[i])))
    {
        i++;
    }

    if (i == 0)
        throw std::runtime_error("Invalid body size syntax (missing digits): " + value);

    std::string numericPart = value.substr(0, i);
    std::string unitPart = value.substr(i);

    size_t baseSize = 0;
    std::istringstream iss(numericPart);
    iss >> baseSize;

    if (iss.fail())
        throw std::runtime_error("Numeric overflow parsing body size base: " + numericPart);
    std::string unit = toUpper(trim(unitPart)); 
    size_t multiplier = 1;

    if (unit.empty() || unit == "B") {
        multiplier = 1;
    } else if (unit == "K" || unit == "KB") {
        multiplier = 1024;
    } else if (unit == "M" || unit == "MB") {
        multiplier = 1024 * 1024;
    } else if (unit == "G" || unit == "GB") {
        multiplier = 1024 * 1024 * 1024;
    } else {
        throw std::runtime_error("Unsupported byte size unit suffix: '" + unitPart + "'");
    }
    if (baseSize > 0 && multiplier > std::numeric_limits<size_t>::max() / baseSize)
    {
        throw std::runtime_error("Calculated client_max_body_size exceeds physical address space limits: " + value);
    }

    return baseSize * multiplier;
}
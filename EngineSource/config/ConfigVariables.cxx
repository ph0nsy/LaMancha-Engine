#include "ConfigVariables.h"
#include "inih/ini.h"


ConfigVariables::ConfigVariables(const std::string& _filename) 
{ 
    m_tokenChar = ",";
    if(!loadINI(_filename)) 
    {
        printf("Error! Couldn't read file");
        return;
    }
}

ConfigVariables::ConfigVariables(const std::string& _filename, char* _tokenChar) 
: m_tokenChar(_tokenChar)
{
    if(!loadINI(_filename)) 
    {
        printf("Error! Couldn't read file");
        return;
    }
}

bool ConfigVariables::loadINI(const std::string& _filename) 
{
    return ini_parse(
        _filename.c_str(), 
        [](void* _user, const char* _section, 
            const char* _name, const char* _value) 
        {
            auto* config = static_cast<ConfigVariables*>(_user);
            std::string key = std::string(_section) + "." + _name;
            config->m_values[key] = _value;
            return 1;
        }, 
        this) == 0;
}

std::string ConfigVariables::getString(const std::string& _section, const std::string& _key) 
{
    std::string full_key = _section + "." + _key;
    auto it = values.find(full_key);
    return (it != values.end()) ? it->second : "";
}

int ConfigVariables::getInt(const std::string& _section, const std::string& _key) 
{
    std::string value = getString(_section, _key);
    return value.empty() ? DEFAULT_V : std::stoi(value);
}

bool ConfigVariables::getBool(const std::string& _section, const std::string& _key) 
{
    std::string value = std::tolower(getString(_section, _key));
    if (value.empty()) { return DEFAULT_V; }
    return (value == "true" || value == "1" || value == "yes");
}

float ConfigVariables::getFloat(const std::string& _section, const std::string& _key) 
{
    std::string value = getString(_section, _key);
    return value.empty() ? DEFAULT_V : std::stof(value);
}

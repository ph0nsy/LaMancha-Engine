#include <string>
#include <unordered_map>

#define DEFAULT_V 0

class iniReader 
{
    std::unordered_map<std::string, std::string> m_values;
    char* m_tokenChar;

    template<typename T>
    static bool tryParseValue(const std::string& _string, T& out)
    {
        if (std::is_arithmetic_v<T>) 
        {
            const char* begin = _string.data();
            const char* end   = _string.data() + _string.size();
            T result = std::from_chars(begin, end, out);
            return result.ec == std::errc() && result.ptr == end;
        } 
        else 
        { 
            std::stringstream stream(_string);
            stream >> out;
            return !stream.fail();
        }
    }

public:

    iniReader(const std::string& _filename);
    iniReader(const std::string& _filename, char* _tokenChar);
    ~iniReader();

    bool loadINI(const std::string& _filename);
    bool unloadINI();
    
    std::string getString(const std::string& _section, const std::string& _key);
    int getInt(const std::string& _section, const std::string& _key);
    bool getBool(const std::string& _section, const std::string& _key);
    float getFloat(const std::string& _section, const std::string& _key);
    
    template<typename T>
    std::vector<T> getVector(const std::string& _section, const std::string& _key)
    {
        std::string sBase = getString(_section, _key);
        if (sBase.empty()) { return {}; }

        std::vector<T> result;
        size_t start = 0;

        while (true) {
            size_t separator = sBase.find(m_tokenChar, start);
            std::string token = (separator == std::string::npos)
                ? sBase.substr(start)
                : sBase.substr(start, separator - start);

            // trim whitespace
            token.erase(0, token.find_first_not_of(" \t"));
            token.erase(token.find_last_not_of(" \t") + 1);

            // parse value
            T value{};
            if (tryParseValue(token, value)) { result.push_back(value); }

            if (separator == std::string::npos) { break; }
            start = separator + 1;
        }

        return result;
    }
};
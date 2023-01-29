#include "utils.hpp"
#include <algorithm>

bool find_string(std::string text, std::string pattern)
{
    if (text.empty() || pattern.empty())
    {
        return pattern.empty();
    }

    std::transform(text.begin(), text.end(), text.begin(), ::tolower);
    std::transform(pattern.begin(), pattern.end(), pattern.begin(), ::tolower);

    return text.find(pattern) != std::string::npos;
}

bool begins_with(const std::string & str, const std::string & prefix)
{
    return prefix.length() <= str.length() &&
           str.substr(0, prefix.length()) == prefix;
}

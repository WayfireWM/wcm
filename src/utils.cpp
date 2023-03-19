#include "utils.hpp"
#include <algorithm>

#include <xkbcommon/xkbregistry.h>

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

using rxkb_context_ptr = std::unique_ptr<rxkb_context, decltype(&rxkb_context_unref)>;

std::map<std::string, std::string> get_xkb_layouts(const std::string& ruleset)
{
    rxkb_context_ptr context =
    {rxkb_context_new(rxkb_context_flags::RXKB_CONTEXT_NO_FLAGS), &rxkb_context_unref};
    if (!rxkb_context_parse(context.get(), ruleset.c_str()))
    {
        return {};
    }

    std::map<std::string, std::string> layouts;
    for (rxkb_layout *layout = rxkb_layout_first(context.get());
         layout != nullptr;
         layout = rxkb_layout_next(layout))
    {
        if (rxkb_layout_get_variant(layout) == nullptr)
        {
            layouts.emplace(rxkb_layout_get_name(layout), rxkb_layout_get_description(layout));
        }
    }

    return layouts;
}

std::map<std::string, std::string> get_xkb_models(const std::string& ruleset)
{
    rxkb_context_ptr context =
    {rxkb_context_new(rxkb_context_flags::RXKB_CONTEXT_NO_FLAGS), &rxkb_context_unref};
    if (!rxkb_context_parse(context.get(), ruleset.c_str()))
    {
        return {};
    }

    std::map<std::string, std::string> models;
    for (rxkb_model *model = rxkb_model_first(context.get());
         model != nullptr;
         model = rxkb_model_next(model))
    {
        models.emplace(rxkb_model_get_name(model), rxkb_model_get_description(model));
    }

    return models;
}

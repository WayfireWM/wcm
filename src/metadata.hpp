#pragma once

#include <gtkmm.h>
#include <string>
#include <variant>
#include <wayfire/config/xml.hpp>

#include "utils.hpp"
#include "wcm.hpp"

enum plugin_type
{
    PLUGIN_TYPE_NONE,
    PLUGIN_TYPE_WAYFIRE,
    PLUGIN_TYPE_WF_SHELL,
};

enum option_type
{
    OPTION_TYPE_UNDEFINED,
    OPTION_TYPE_INT,
    OPTION_TYPE_BOOL,
    OPTION_TYPE_DOUBLE,
    OPTION_TYPE_STRING,
    OPTION_TYPE_GESTURE,
    OPTION_TYPE_ACTIVATOR,
    OPTION_TYPE_BUTTON,
    OPTION_TYPE_KEY,
    OPTION_TYPE_COLOR,
    OPTION_TYPE_GROUP,
    OPTION_TYPE_SUBGROUP,
    OPTION_TYPE_DYNAMIC_LIST,
    OPTION_TYPE_ANIMATION,
};

enum mod_type
{
    MOD_TYPE_NONE    = 0,
    MOD_TYPE_SHIFT   = 1 << 0,
    MOD_TYPE_CONTROL = 1 << 1,
    MOD_TYPE_ALT     = 1 << 2,
    MOD_TYPE_SUPER   = 1 << 3,
};

enum hint_type
{
    HINT_FILE      = 1,
    HINT_DIRECTORY = 2,
};

struct LabeledInt
{
    int value;
    std::string name;
};

struct LabeledString
{
    int id;
    std::string value;
    std::string name;
};

struct var_data
{
    double min;
    double max;
    double precision;
    hint_type hints;
};

using opt_data = std::variant<int, std::string, double>;

class Plugin;

class Option
{
    template<class value_type>
    void set_value(wf_section section, const value_type & value);

    template<class... ArgTypes>
    void set_value(wf_section section, ArgTypes... args)
    {
        throw std::logic_error("Unimplemented");
    }

  public:
    Option(xmlNode *cur_node, Plugin *plugin);
    Option(option_type group_type, Plugin *plugin);
    Option() = default;
    ~Option();

    Option *create_child_option(const std::string & name, option_type type);

    Plugin *plugin;
    std::string name;
    std::string disp_name;
    std::string tooltip;
    std::string binding;
    option_type type;
    mod_type mod_mask;
    opt_data default_value;
    var_data data;
    Option *parent;
    bool hidden = false;

    std::vector<Option*> options;
    std::vector<std::pair<std::string, int>> int_labels;
    std::vector<std::pair<std::string, std::string>> str_labels;

    template<class... ArgTypes>
    void set_save(const ArgTypes &... args);
};

class WCM;

class Plugin
{
  public:
    std::string name;
    std::string disp_name;
    std::string tooltip;
    std::string category;
    plugin_type type;
    bool enabled;
    std::vector<Option*> option_groups;

    // widget of the plugin which is shown on the main page
    Gtk::Box widget = Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 5);
    Gtk::CheckButton enabled_check;
    Gtk::Button button;
    Gtk::Box button_layout = Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 5);
    Gtk::Image icon;
    Gtk::Label label;

    static Plugin *get_plugin_data(xmlNode *node, Option *main_group = nullptr,
        Plugin *plugin = nullptr);
    void init_widget();
    inline bool is_core_plugin()
    {
        return name == "core" || name == "input" || name == "workarounds";
    }
};

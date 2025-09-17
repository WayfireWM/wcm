/*
 * Copyright © 2020 Scott Moreau
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#pragma once

#include <algorithm>
#include <array>
#include <gdk/gdkwayland.h>
#include <gtkmm.h>
#include <iostream>
#include <fmt/core.h>
#include <libintl.h>
#include <variant>
#include <vector>
#include <wayfire/config/file.hpp>
#include <wayfire/config/xml.hpp>
#include <keyboard-shortcuts-inhibit-unstable-v1-client-protocol.h>
#include <glibmm/i18n.h>

#include "metadata.hpp"

struct animate_option
{
    Option *option;
    Gtk::SpinButton *spin_button;
    Gtk::ComboBoxText *combo_box;
};

class MainPage : public Gtk::ScrolledWindow
{
  public:
    static const int NUM_CATEGORIES = 8;
    MainPage(const std::vector<Plugin*> & plugins);
    void set_filter(const Glib::ustring & filter);

  private:
    struct Category
    {
        Glib::ustring name;

        Gtk::Box vbox = Gtk::Box(Gtk::ORIENTATION_VERTICAL, 10);

        Gtk::Box title_box = Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 10);
        Gtk::Image image;
        Gtk::Label label;

        Gtk::FlowBox flowbox;

        Category(const Glib::ustring & name_string, const Glib::ustring & icon_name);
        void add_plugin(Plugin *plugin);
    };

    const std::vector<Plugin*> & plugins;
    Gtk::Box vbox = Gtk::Box(Gtk::ORIENTATION_VERTICAL, 10);
    Glib::RefPtr<Gtk::SizeGroup> size_group = Gtk::SizeGroup::create(
        Gtk::SIZE_GROUP_BOTH);
    std::array<Gtk::Separator, NUM_CATEGORIES - 1> separators;
    std::array<Category, NUM_CATEGORIES> categories = {
        Category{_("General"), "preferences-system"},
        {_("Accessibility"), "preferences-desktop-accessibility"},
        {_("Desktop"), "preferences-desktop"}, {_("Shell"), "user-desktop"},
        {_("Effects"), "applications-graphics"},
        {_("Window Management"), "applications-accessories"},
        {_("Utility"), "applications-other"}, {_("Other"), "applications-other"}};
};

class KeyEntry : public Gtk::Stack
{
    Gtk::Box grab_layout  = Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 10);
    Gtk::Label grab_label = Gtk::Label(_("(none)"));
    Gtk::Button grab_button;
    Gtk::Button edit_button;

    Gtk::Box edit_layout = Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 10);
    Gtk::Entry entry;
    Gtk::Button ok_button;
    Gtk::Button cancel_button;

    static mod_type get_mod_from_keyval(guint keyval);
    static bool check_and_confirm(const std::string & key_str);
    static std::string grab_key();

    sigc::signal<void> changed;

  public:
    KeyEntry();
    inline sigc::signal<void> signal_changed()
    {
        return changed;
    }

    inline std::string get_value() const
    {
        return grab_label.get_text();
    }

    void set_value(const std::string & value)
    {
        grab_label.set_label(value);
        entry.set_text(value);
        changed.emit();
    }
};

class LayoutsEntry : public Gtk::Entry
{
    Gtk::SeparatorMenuItem separator;
    std::vector<Gtk::MenuItem> layouts;

  public:
    LayoutsEntry();
};

class XkbModelEntry : public Gtk::Entry
{
    Gtk::SeparatorMenuItem separator;
    std::vector<Gtk::MenuItem> models;

  public:
    XkbModelEntry();
};

class OptionWidget : public Gtk::Box
{
    Gtk::Label name_label;
    std::vector<std::unique_ptr<Gtk::Widget>> widgets;
    Gtk::Button reset_button;
    sigc::trackable tracker;

    guint int_sb_handle;
    guint animate_sb_handle;
    guint animate_cb_handle;
    std::unique_ptr<Gtk::SpinButton> animate_spin_button, int_spin_button;
    std::unique_ptr<Gtk::ComboBoxText> animate_combo_box;
    animate_option ao;

    inline void pack_end(std::unique_ptr<Gtk::Widget> && widget, bool expand = false,
        bool fill = false)
    {
        Gtk::Box::pack_end(*widget, expand, fill);
        widgets.push_back(std::move(widget));
    }

  public:
    OptionWidget(Option *option);
    ~OptionWidget();
};

class DynamicListBase : public Gtk::Box
{
  protected:
    Gtk::Box add_box = Gtk::Box(Gtk::ORIENTATION_HORIZONTAL);
    Gtk::Button add_button;

    std::vector<std::unique_ptr<Gtk::Widget>> widgets;
    inline void pack_widget(std::unique_ptr<Gtk::Widget> && widget)
    {
        pack_start(*widget, false, false);
        widgets.push_back(std::move(widget));
    }

    inline void remove(Gtk::Widget *widget)
    {
        widgets.erase(std::find_if(widgets.begin(), widgets.end(),
            [=] (std::unique_ptr<Gtk::Widget> & w) { return w.get() == widget; }));
    }

    DynamicListBase();
};

class AutostartDynamicList : public DynamicListBase
{
    struct AutostartWidget : public Gtk::Box
    {
        Gtk::Entry command_entry;
        Gtk::Button choose_button;
        Gtk::Button run_button;
        Gtk::Button remove_button;

        AutostartWidget(Option *option);
    };

  public:
    explicit AutostartDynamicList(Option *option);
};

class BindingsDynamicList : public DynamicListBase
{
    struct BindingWidget : public Gtk::Frame
    {
        Gtk::Expander expander = Gtk::Expander(_("Command:"));
        Gtk::Box vbox     = Gtk::Box(Gtk::ORIENTATION_VERTICAL, 10);
        Gtk::Box type_box = Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 10);
        Gtk::Box binding_box = Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 10);
        Gtk::Box command_box = Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 10);

        Gtk::Label type_label    = Gtk::Label(_("Type"));
        Gtk::Label binding_label = Gtk::Label(_("Binding"));
        Gtk::Label command_label = Gtk::Label(_("Command"));

        Gtk::ComboBoxText type_combo_box;
        std::unique_ptr<KeyEntry> key_entry;
        Gtk::Entry command_entry;
        Gtk::Button remove_button;

        std::shared_ptr<wf::config::option_base_t> binding_wf_opt;

        BindingWidget(const std::string & cmd_name, Option *option,
            wf_section section);
    };

  public:
    explicit BindingsDynamicList(Option *option);
};

enum class VswitchBindingKind
{
    WITHOUT_WINDOW,
    WITH_WINDOW,
    SEND_WINDOW,
};

template<enum VswitchBindingKind>
class VswitchBindingsDynamicList : public DynamicListBase
{
    static const std::string OPTION_PREFIX;

    struct BindingWidget : public Gtk::Box
    {
        Gtk::Label label = Gtk::Label(_("Workspace"));
        Gtk::SpinButton workspace_spin_button = Gtk::SpinButton(Gtk::Adjustment::create(1, 1, 30));
        KeyEntry key_entry;
        Gtk::Button remove_button;

        std::shared_ptr<wf::config::option_base_t> binding_wf_opt;

        BindingWidget(std::shared_ptr<wf::config::section_t> section, Option *option, int workspace_index);
    };

  public:
    explicit VswitchBindingsDynamicList(Option *option);
};

template<enum VswitchBindingKind kind>
class VswitchBindingsWidget : public Gtk::Frame
{
    VswitchBindingsDynamicList<kind> dynamic_list;

    static const Glib::ustring LABEL_TEXT;

  public:
    explicit VswitchBindingsWidget(Option *option);
};

class OptionSubgroupWidget : public Gtk::Frame
{
    Gtk::Expander expander;
    Gtk::Box expander_layout = Gtk::Box(Gtk::ORIENTATION_VERTICAL, 10);
    std::vector<std::unique_ptr<OptionWidget>> option_widgets;

  public:
    OptionSubgroupWidget(Option *subgroup);
};

class OptionGroupWidget : public Gtk::ScrolledWindow
{
    Gtk::Box options_layout = Gtk::Box(Gtk::ORIENTATION_VERTICAL, 10);
    std::vector<std::unique_ptr<Gtk::Widget>> option_widgets;

  public:
    OptionGroupWidget(Option *group);
};

class PluginPage : public Gtk::Notebook
{
    std::vector<OptionGroupWidget> groups;

  public:
    PluginPage(Plugin *plugin);
};

class WCM
{
  private:
    void parse_config(wf::config::config_manager_t & config_manager);
    bool init_input_inhibitor();
    void create_main_layout();
    void save_to_file(wf::config::config_manager_t & mgr, const std::string & file);

    // these objects can be used when widgets are destroyed and emit `signal_changed`
    // causing saving config
    // so these objects should be destroyed after widgets
    wf::config::config_manager_t wf_config_mgr;
    wf::config::config_manager_t wf_shell_config_mgr;
    std::string wf_config_file;
    std::string wf_shell_config_file;
    std::string start_plugin;
    std::vector<Plugin*> plugins;

    Plugin *current_plugin = nullptr;

    std::unique_ptr<Gtk::ApplicationWindow> window;
    Gtk::Box global_layout;

    Gtk::Stack main_stack; /* for animated transition */
    std::unique_ptr<MainPage> main_page;
    std::unique_ptr<PluginPage> plugin_page;

    Gtk::Stack left_stack; /* for animated transition */

    Gtk::Box main_left_panel_layout = Gtk::Box(Gtk::ORIENTATION_VERTICAL);
    Gtk::Label filter_label;
    Gtk::SearchEntry search_entry;
    PrettyButton close_button = PrettyButton(_("Close"), "window-close");
    PrettyButton output_config_button =
        PrettyButton(_("Configure Outputs"), "computer");

    Gtk::Box plugin_left_panel_layout = Gtk::Box(Gtk::ORIENTATION_VERTICAL);
    Gtk::Label plugin_name_label;
    Gtk::Label plugin_description_label;
    Gtk::Box plugin_enabled_box = Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 10);
    Gtk::CheckButton plugin_enabled_check;
    Gtk::Label plugin_enabled_label = Gtk::Label(_("Use This Plugin"));
    PrettyButton back_button = PrettyButton(_("Back"), "go-previous");

    cairo_surface_t *grab_window_surface = nullptr;
    zwp_keyboard_shortcuts_inhibitor_v1 *shortcuts_inhibitor     = nullptr;
    zwp_keyboard_shortcuts_inhibit_manager_v1 *inhibitor_manager = nullptr;

    // WCM is a singleton
    static inline WCM *instance = nullptr;

  public:
    WCM(Glib::RefPtr<Gtk::Application> app);
    static inline WCM *get_instance()
    {
        if (instance == nullptr)
        {
            throw std::logic_error(
                "Cannot get an instance of WCM before it's initialized");
        }

        return instance;
    }

    void open_page(Plugin *plugin = nullptr);

    void set_plugin_enabled(Plugin *plugin, bool enabled);
    std::string find_icon(const std::string & icon_name);

    void load_config_files();
    inline void parse_config()
    {
        parse_config(wf_config_mgr);
    }

    std::shared_ptr<wf::config::section_t> get_config_section(Plugin *plugin);
#if HAVE_WFSHELL
    inline void parse_wfshell_config()
    {
        parse_config(wf_shell_config_mgr);
    }

#endif
    bool save_config(Plugin *plugin);

    inline std::string get_xkb_rules()
    {
        return wf_config_mgr.get_section("input")->get_option("xkb_rules")->get_value_str();
    }

    inline void set_inhibitor_manager(zwp_keyboard_shortcuts_inhibit_manager_v1 *value)
    {
        inhibitor_manager = value;
    }

    bool lock_input(Gtk::Dialog *grab_dialog);
    void unlock_input();
    Plugin *find_plugin_by_name(std::vector<Plugin*> plugins, std::string search_name);
};

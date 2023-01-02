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

#ifndef WCM_HPP
#define WCM_HPP

#include <algorithm>
#include <gdk/gdkwayland.h>
#include <gtkmm.h>
#include <iostream>
#include <string.h>
#include <variant>
#include <vector>
#include <wayfire/config/file.hpp>
#include <wayfire/config/xml.hpp>
#include <wlr-input-inhibitor-unstable-v1-client-protocol.h>

#include "metadata.hpp"

class MainPage : public Gtk::ScrolledWindow
{
    public:
    static const int NUM_CATEGORIES = 8;
    MainPage(const std::vector<Plugin *> &plugins);
    void set_filter(const Glib::ustring &filter);

    private:
    struct Category
    {
        Glib::ustring name;

        Gtk::Box vbox = Gtk::Box(Gtk::ORIENTATION_VERTICAL, 10);

        Gtk::Box title_box = Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 10);
        Gtk::Image image;
        Gtk::Label label;

        Gtk::FlowBox flowbox;

        Category(const Glib::ustring &name_string, const Glib::ustring &icon_name);
        void add_plugin(Plugin *plugin);
    };

    const std::vector<Plugin *> &plugins;
    Gtk::Box vbox = Gtk::Box(Gtk::ORIENTATION_VERTICAL);
    Glib::RefPtr<Gtk::SizeGroup> size_group = Gtk::SizeGroup::create(Gtk::SIZE_GROUP_BOTH);
    std::array<Gtk::Separator, NUM_CATEGORIES - 1> separators;
    std::array<Category, NUM_CATEGORIES> categories = {
        Category{"General", "preferences-system"}, {"Accessibility", "preferences-desktop-accessibility"},
        {"Desktop", "preferences-desktop"},        {"Shell", "user-desktop"},
        {"Effects", "applications-graphics"},      {"Window Management", "applications-accessories"},
        {"Utility", "applications-other"},         {"Other", "applications-other"}};
};

class OptionWidget : public Gtk::Box
{
    Gtk::Label name_label;
    std::vector<std::unique_ptr<Gtk::Widget>> widgets;
    Gtk::Button reset_button;

    public:
    OptionWidget(Option *option);
    inline void pack_end(std::unique_ptr<Gtk::Widget> &&widget, bool expand = false, bool fill = false)
    {
        Gtk::Box::pack_end(*widget, expand, fill);
        widgets.push_back(std::move(widget));
    }
};

class OptionSubgroupWidget : public Gtk::Frame
{
    Gtk::Expander expander;
    Gtk::Box expander_layout = Gtk::Box(Gtk::ORIENTATION_VERTICAL, 10);
    std::vector<OptionWidget> option_widgets;

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
    void parse_config(wf::config::config_manager_t &config_manager);
    bool init_input_inhibitor();
    void create_main_layout();

    Plugin *current_plugin = nullptr;

    Gtk::ApplicationWindow window;
    Gtk::Box global_layout;

    Gtk::Stack main_stack; /* for animated transition */
    std::unique_ptr<MainPage> main_page;
    std::unique_ptr<PluginPage> plugin_page;

    Gtk::Stack left_stack; /* for animated transition */

    Gtk::Box main_left_panel_layout = Gtk::Box(Gtk::ORIENTATION_VERTICAL);
    Gtk::Label filter_label;
    Gtk::Entry search_entry;
    PrettyButton close_button = PrettyButton("Close", "window-close");
    PrettyButton output_config_button = PrettyButton("Configure Outputs", "computer");

    Gtk::Box plugin_left_panel_layout = Gtk::Box(Gtk::ORIENTATION_VERTICAL);
    Gtk::Label plugin_name_label;
    Gtk::Label plugin_description_label;
    Gtk::Box plugin_enabled_box = Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 10);
    Gtk::CheckButton plugin_enabled_button;
    Gtk::Label plugin_enabled_label = Gtk::Label("Use This Plugin");
    PrettyButton back_button = PrettyButton("Back", "go-previous");

    std::vector<Plugin *> plugins;
    wf::config::config_manager_t wf_config_mgr;
    wf::config::config_manager_t wf_shell_config_mgr;
    const char *wf_config_file;
    const char *wf_shell_config_file;
    cairo_surface_t *grab_window_surface;
    zwlr_input_inhibitor_v1 *screen_lock;
    zwlr_input_inhibit_manager_v1 *inhibitor_manager;

    // WCM is a singleton
    static inline WCM *instance = nullptr;

    public:
    WCM(Glib::RefPtr<Gtk::Application> app);
    static inline WCM *get_instance()
    {
        if (instance == nullptr)
            throw std::logic_error("Cannot get an instance of WCM before it's initialized");
        return instance;
    }
    void open_page(Plugin *plugin = nullptr);
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
    inline void set_inhibitor_manager(zwlr_input_inhibit_manager_v1 *value)
    {
        inhibitor_manager = value;
    }
};

#endif

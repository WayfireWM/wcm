#include "wcm.hpp"
#include "utils.hpp"

#include <wayfire/config/compound-option.hpp>
#include <wayfire/config/types.hpp>
#include <wayfire/config/xml.hpp>
#include <wordexp.h>

#define OUTPUT_CONFIG_PROGRAM "wdisplays"

OptionGroupWidget::OptionGroupWidget(Option *group)
{
    add(options_layout);
    set_vexpand();

    for (Option *option : group->options)
    {
        if (option->hidden)
        {
            continue;
        }

        if (option->type == OPTION_TYPE_SUBGROUP && !option->options.empty())
        {
            option_widgets.push_back(std::make_unique<OptionSubgroupWidget>(option));
        }
        else if (option->type == OPTION_TYPE_DYNAMIC_LIST)
        {
            option_widgets.push_back(std::make_unique<OptionDynamicListWidget>(option));
        }
        else
        {
            option_widgets.push_back(std::make_unique<OptionWidget>(option));
        }
        options_layout.pack_start(*option_widgets.back(), false, false);
    }
}

std::ostream &operator<<(std::ostream &out, const wf::color_t &color)
{
    return out << "rgba(" << color.r << ", " << color.g << ", " << color.b << ", " << color.a << ")";
}

template <class value_type>
void Option::set_value(wf_section section, const value_type &value)
{
    std::cout << name << " = " << value << ": ";
    section->get_option_or(name)->set_value_str(wf::option_type::to_string<value_type>(value));
}

template <class... ArgTypes>
void Option::set_save(const ArgTypes &...args)
{
    auto section = WCM::get_instance()->get_config_section(plugin);
    if (!section)
        return;

    std::cout << __PRETTY_FUNCTION__ << ": \n  ";
    set_value(section, args...);
    WCM::get_instance()->save_config(plugin);
}

OptionWidget::OptionWidget(Option *option) : Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 10)
{
    name_label.set_text(option->disp_name);
    name_label.set_tooltip_markup(option->tooltip);
    name_label.set_size_request(200);
    name_label.set_alignment(Gtk::ALIGN_START);

    reset_button.set_image_from_icon_name("edit-clear");
    reset_button.set_tooltip_text("Reset to default");

    pack_start(name_label, false, false);
    Gtk::Box::pack_end(reset_button, false, false);

    std::shared_ptr<wf::config::section_t> section = WCM::get_instance()->get_config_section(option->plugin);
    std::shared_ptr<wf::config::option_base_t> wf_option = section->get_option(option->name);

    switch (option->type)
    {
        case OPTION_TYPE_INT:
        {
            auto value_optional = wf::option_type::from_string<int>(wf_option->get_value_str());
            if (!value_optional)
                return;
            int value = value_optional.value();

            if (option->int_labels.empty())
            {
                auto spin_button = std::make_unique<Gtk::SpinButton>(
                    Gtk::Adjustment::create(value, option->data.min, option->data.max, 1));
                spin_button->signal_value_changed().connect(
                    [=, widget = spin_button.get()] { option->set_save(widget->get_value_as_int()); });
                pack_end(std::move(spin_button));
            }
            else
            {
                auto combo_box = std::make_unique<Gtk::ComboBoxText>();
                for (const auto &[name, _] : option->int_labels)
                {
                    combo_box->append(name);
                }
                combo_box->set_active(value);
                combo_box->signal_changed().connect([=, widget = combo_box.get()] {
                    option->set_save(option->int_labels.at(widget->get_active_text()));
                });
                pack_end(std::move(combo_box), true, true);
            }
        }
        break;

        case OPTION_TYPE_BOOL:
        {
            auto value_optional = wf::option_type::from_string<bool>(wf_option->get_value_str());
            if (!value_optional)
                return;
            bool value = value_optional.value();

            auto check_button = std::make_unique<Gtk::CheckButton>();
            check_button->set_active(value);
            check_button->signal_toggled().connect(
                [=, widget = check_button.get()] { option->set_save(widget->get_active()); });
            pack_end(std::move(check_button));
        }
        break;

        case OPTION_TYPE_DOUBLE:
        {
            auto value_optional = wf::option_type::from_string<double>(wf_option->get_value_str());
            if (!value_optional)
                return;
            double value = value_optional.value();

            auto spin_box = std::make_unique<Gtk::SpinButton>(
                Gtk::Adjustment::create(value, option->data.min, option->data.max, option->data.precision),
                option->data.precision, 3);
            spin_box->signal_changed().connect([=, widget = spin_box.get()] { option->set_save(widget->get_value()); });
            pack_end(std::move(spin_box));
        }
        break;

        case OPTION_TYPE_ACTIVATOR:
        case OPTION_TYPE_BUTTON:
        case OPTION_TYPE_KEY:
        {
            auto key_grab_button = std::make_unique<Gtk::Button>();
            auto edit_button = std::make_unique<Gtk::Button>();
            edit_button->set_image_from_icon_name("gtk-edit", Gtk::ICON_SIZE_BUTTON);
            edit_button->set_tooltip_text("Edit binding");
            edit_button->signal_clicked().connect([=] {});
            key_grab_button->set_label(section->get_option(option->name)->get_value_str());
            pack_end(std::move(edit_button));
            pack_end(std::move(key_grab_button), true, true);
        }
        break;

        case OPTION_TYPE_GESTURE:
        case OPTION_TYPE_STRING:
        {
            if (option->str_labels.empty())
            {
                auto entry = std::make_unique<Gtk::Entry>();
                entry->set_text(wf_option->get_value_str());
                entry->signal_changed().connect(
                    [=, widget = entry.get()] { option->set_save<std::string>(widget->get_text()); });

                if (option->data.hints & HINT_DIRECTORY)
                {
                    auto dir_choose_button = std::make_unique<Gtk::Button>();
                    dir_choose_button->set_image_from_icon_name("folder-open");
                    dir_choose_button->set_image_from_icon_name("Choose Directory");
                    dir_choose_button->signal_clicked().connect([=] { /* TODO */ });
                    pack_end(std::move(dir_choose_button));
                }
                if (option->data.hints & HINT_FILE)
                {
                    auto file_choose_button = std::make_unique<Gtk::Button>();
                    file_choose_button->set_image_from_icon_name("text-x-generic");
                    file_choose_button->set_image_from_icon_name("Choose File");
                    file_choose_button->signal_clicked().connect([=] { /* TODO */ });
                    pack_end(std::move(file_choose_button));
                }

                pack_end(std::move(entry), true, true);
            }
            else
            {
                auto combo_box = std::make_unique<Gtk::ComboBoxText>();
                for (const auto &[name, str_value] : option->str_labels)
                {
                    combo_box->append(name);
                    if (str_value == wf_option->get_default_value_str())
                        combo_box->set_active_text(name);
                }
                combo_box->signal_changed().connect([=, widget = combo_box.get()] {
                    option->set_save(option->str_labels.at(widget->get_active_text()));
                });
                pack_end(std::move(combo_box), true, true);
            }
        }
        break;

        case OPTION_TYPE_COLOR:
        {
            auto value_optional = wf::option_type::from_string<wf::color_t>(wf_option->get_value_str());
            if (!value_optional)
                return;
            wf::color_t value = value_optional.value();

            Gdk::RGBA rgba;
            rgba.set_red(value.r);
            rgba.set_green(value.g);
            rgba.set_blue(value.b);
            rgba.set_alpha(value.a);
            auto color_button = std::make_unique<Gtk::ColorButton>(rgba);
            color_button->set_title(option->disp_name);
            color_button->signal_color_set().connect([=, widget = color_button.get()] {
                auto rgba = widget->get_rgba();
                wf::color_t color = {rgba.get_red(), rgba.get_green(), rgba.get_blue(), rgba.get_alpha()};
                option->set_save(color);
            });
            pack_end(std::move(color_button));
        }
        break;

        default:
            break;
    }
}

OptionDynamicListWidget::AutostartWidget::AutostartWidget(Option *option) : Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 10)
{
    command_entry.set_text(std::get<std::string>(option->default_value));
    command_entry.signal_changed().connect([=] { option->set_save<std::string>(command_entry.get_text()); });
    choose_button.set_image_from_icon_name("application-x-executable");
    choose_button.signal_clicked().connect([=] { /* TODO */ });
    run_button.set_image_from_icon_name("media-playback-start");
    run_button.signal_clicked().connect([=] { Glib::spawn_command_line_async(command_entry.get_text()); });
    remove_button.set_image_from_icon_name("list-remove");
    remove_button.signal_clicked().connect([=] { /* TODO */ });
    pack_start(command_entry, true, true);
    pack_start(choose_button, false, false);
    pack_start(run_button, false, false);
    pack_start(remove_button, false, false);
}

OptionDynamicListWidget::CommandWidget::CommandWidget()
{
    add(expander);
    expander.set_label(/* TODO */ "");
    expander.add(vbox);

    type_label.set_size_request(200);
    type_box.pack_start(type_label, false, false);
    type_combo_box.signal_changed().connect([=] { /* TODO */ });
    type_box.pack_start(type_combo_box, true, true);
    vbox.pack_start(type_box, false, false);

    binding_label.set_size_request(200);
    binding_box.pack_start(binding_label);
    binding_button.signal_clicked().connect([=] { /* TODO */ });
    binding_box.pack_start(binding_button, true, true);
    binding_edit_button.set_image_from_icon_name("gtk-edit");
    binding_edit_button.signal_clicked().connect([=] { /*  TODO */ });
    binding_box.pack_start(binding_edit_button, false, false);
    vbox.pack_start(binding_box, false, false);

    command_label.set_size_request(200);
    command_box.pack_start(command_label, false, false);
    command_entry.signal_changed().connect([=] { /* TODO */ });
    command_box.pack_start(command_entry, true, true);
    remove_button.set_image_from_icon_name("list-remove");
    command_box.pack_start(remove_button, false, false);
    vbox.pack_start(command_box);
}

OptionDynamicListWidget::OptionDynamicListWidget(Option *option) : Gtk::Box(Gtk::ORIENTATION_VERTICAL, 10)
{
    std::cout << option->name << std::endl;

    if (option->name == "bindings")
    {
    }
    else if (option->name == "autostart")
    {
        std::shared_ptr<wf::config::section_t> section = WCM::get_instance()->get_config_section(option->plugin);
        if (!section)
            return;

        auto wf_option = std::dynamic_pointer_cast<wf::config::compound_option_t>(section->get_option("autostart"));
        auto autostart_names = wf_option->get_value<std::string>();
        option->options.clear();

        for (const auto &[opt_name, executable] : autostart_names)
        {
            if (std::get<std::string>(option->default_value) != "string")
                continue;

            Option *dyn_opt = new Option();
            dyn_opt->name = opt_name;
            dyn_opt->type = OPTION_TYPE_STRING;
            dyn_opt->parent = option;
            dyn_opt->plugin = option->plugin;
            dyn_opt->default_value = executable;
            option->options.push_back(dyn_opt);

            auto widget = std::make_unique<AutostartWidget>(dyn_opt);
            pack_start(*widget);
            widgets.push_back(std::move(widget));
        }
    }
    else
    {
        return;
    }

    add_button.set_image_from_icon_name("list-add");
    add_button.signal_clicked().connect([=] { /* TODO */ });

    add_box.pack_end(add_button, false, false);
    pack_end(add_box, true, false);
}

OptionSubgroupWidget::OptionSubgroupWidget(Option *subgroup)
{
    add(expander);
    expander.set_label(subgroup->name);
    expander.add(expander_layout);
    for (Option *option : subgroup->options)
    {
        option_widgets.push_back(OptionWidget(option));
        expander_layout.pack_start(option_widgets.back());
    }
}

PluginPage::PluginPage(Plugin *plugin)
{
    for (auto *group : plugin->option_groups)
    {
        if (group->type != OPTION_TYPE_GROUP || group->hidden)
        {
            continue;
        }
        groups.push_back(OptionGroupWidget(group));
        append_page(groups.back(), group->name);
        groups.back().property_margin().set_value(10);
    }
};

MainPage::Category::Category(const Glib::ustring &name, const Glib::ustring &icon_name) : name(name)
{
    label.set_markup("<span size=\"14000\"><b>" + name + "</b></span>");
    image.set_from_icon_name(icon_name, Gtk::ICON_SIZE_DND);
    title_box.pack_start(image);
    title_box.pack_start(label);
    title_box.set_halign(Gtk::ALIGN_START);
    vbox.pack_start(title_box);
    vbox.pack_start(flowbox);
    vbox.set_margin_top(10);
    vbox.set_margin_bottom(10);
    flowbox.set_selection_mode(Gtk::SELECTION_NONE);
}

void Plugin::init_widget()
{
    label.set_text(disp_name);
    label.set_ellipsize(Pango::ELLIPSIZE_END);
    icon.set(ICONDIR "/plugin-" + name + ".svg");
    button_layout.pack_start(icon);
    button_layout.pack_start(label);
    button_layout.set_halign(Gtk::ALIGN_START);
    button.set_tooltip_markup(tooltip);
    button.set_relief(Gtk::RELIEF_NONE);
    button.add(button_layout);
    enabled_check.set_active(enabled);
    widget.set_halign(Gtk::ALIGN_START);
    widget.pack_start(enabled_check, false, false);
    if (!is_core_plugin() && type == PLUGIN_TYPE_WAYFIRE)
    {
        enabled_check.signal_toggled().connect([=] {});
    }
    else
    {
        enabled_check.set_sensitive(false);
        // enabled_check.set_opacity(0);
    }
    widget.pack_start(button);
    button.signal_clicked().connect([=] { WCM::get_instance()->open_page(this); });
}

void MainPage::Category::add_plugin(Plugin *plugin)
{
    plugin->init_widget();
    flowbox.add(plugin->widget);
}

MainPage::MainPage(const std::vector<Plugin *> &plugins) : plugins(plugins)
{
    add(vbox);
    for (auto *plugin : plugins)
    {
        std::find_if(categories.begin(), categories.end() - 1, [=](const Category &cat) {
            return cat.name == plugin->category;
        })->add_plugin(plugin);
        size_group->add_widget(plugin->widget);
    }
    vbox.add(categories[0].vbox);
    for (int i = 1; i < NUM_CATEGORIES; ++i)
    {
        vbox.add(separators[i - 1]);
        vbox.add(categories[i].vbox);
    }
    // hide empty categories
    signal_show().connect([=] { set_filter(""); });
}

void MainPage::set_filter(const Glib::ustring &filter)
{
    std::map<std::string, bool> category_visible;
    for (const auto &cat : categories)
        category_visible.insert({cat.name, false});

    for (auto *plug : plugins)
    {
        bool plug_visible = find_string(plug->name, filter) || find_string(plug->disp_name, filter) ||
                            find_string(plug->tooltip, filter);
        // the parent of `plug->widget` is `Gtk::FlowBoxItem`
        plug->widget.get_parent()->set_visible(plug_visible);
        category_visible[plug->category] |= plug_visible;
    }

    categories[0].vbox.set_visible(category_visible[categories[0].name]);
    for (int i = 0; i < NUM_CATEGORIES - 1; ++i)
    {
        separators[i].set_visible(category_visible[categories[i].name]);
        categories[i + 1].vbox.set_visible(category_visible[categories[i + 1].name]);
    }
}

void WCM::parse_config(wf::config::config_manager_t &config_manager)
{
    for (auto &s : config_manager.get_all_sections())
    {
        xmlNode *root_element = wf::config::xml::get_section_xml_node(s);

        if (!root_element)
        {
            continue;
        }

        root_element = root_element->parent;
        std::string root_name = (char *)root_element->name;

        if (root_element->type == XML_ELEMENT_NODE && (root_name == "wayfire" || root_name == "wf-shell"))
        {
            printf("Loading %s plugin: %s\n", root_element->name, s->get_name().c_str());
            Plugin *p = Plugin::get_plugin_data(root_element);
            if (p)
            {
                plugins.push_back(p);
            }
            else
            {
                continue;
            }

            if (root_name == "wayfire")
            {
                p->type = PLUGIN_TYPE_WAYFIRE;
            }
            else if (root_name == "wf-shell")
            {
                p->type = PLUGIN_TYPE_WF_SHELL;
            }
            else
            {
                // Should be unreachable because `root_name` is "wayfire" or "wf-shell"
                p->type = PLUGIN_TYPE_NONE;
            }
        }
    }
}

std::string::size_type find_plugin(Plugin *p, const std::string &plugins)
{
    char c1 = 0, c2 = 0;
    std::string::size_type pos = 0;

    while (true)
    {
        pos = plugins.find(p->name, pos);

        if (pos == std::string::npos)
        {
            break;
        }

        c1 = plugins[std::max((int)pos - 1, 0)];
        c2 = plugins[pos + p->name.length()];

        if (((c1 == ' ') || (c1 == 0) || (pos == 0)) && ((c2 == ' ') || (c2 == 0)))
        {
            return pos;
        }

        pos += p->name.length();
    }

    return std::string::npos;
}

bool plugin_enabled(Plugin *p, const std::string &plugins)
{
    return p->is_core_plugin() || p->type == PLUGIN_TYPE_WF_SHELL || find_plugin(p, plugins) != std::string::npos;
}

WCM::WCM(Glib::RefPtr<Gtk::Application> app) : window(Gtk::ApplicationWindow(app))
{
    if (instance)
        throw std::logic_error("WCM should not be initialized more than once");
    instance = this;
    load_config_files();
    parse_config();
#if HAVE_WFSHELL
    parse_wfshell_config();
#endif

    if (!init_input_inhibitor())
    {
        std::cerr << "Binding grabs will not work" << std::endl;
    }

    auto section = wf_config_mgr.get_section("core");
    auto option = section->get_option("plugins");
    auto plugins = option->get_value_str();
    for (auto *plugin : this->plugins)
    {
        plugin->enabled = plugin_enabled(plugin, plugins);
    }

    auto icon = Gdk::Pixbuf::create_from_file(ICONDIR "/wcm.png");
    window.set_icon(icon);
    window.set_size_request(750, 550);
    window.set_default_size(1200, 580);
    window.set_title("Wayfire Config Manager");
    create_main_layout();
    window.show_all();
}

static void registry_add_object(void *data, struct wl_registry *registry, uint32_t name, const char *interface,
                                uint32_t version)
{
    WCM *wcm = (WCM *)data;

    if (strcmp(interface, zwlr_input_inhibit_manager_v1_interface.name) == 0)
    {
        wcm->set_inhibitor_manager((zwlr_input_inhibit_manager_v1 *)wl_registry_bind(
            registry, name, &zwlr_input_inhibit_manager_v1_interface, 1u));
    }
}

static void registry_remove_object(void *data, struct wl_registry *registry, uint32_t name)
{
}

static struct wl_registry_listener registry_listener = {&registry_add_object, &registry_remove_object};

bool WCM::init_input_inhibitor()
{
    struct wl_display *display = gdk_wayland_display_get_wl_display(gdk_display_get_default());
    if (!display)
    {
        std::cerr << "Failed to acquire wl_display for input inhibitor" << std::endl;

        return false;
    }

    struct wl_registry *registry = wl_display_get_registry(display);
    if (!registry)
    {
        std::cerr << "Failed to acquire wl_registry for input inhibitor" << std::endl;

        return false;
    }

    wl_registry_add_listener(registry, &registry_listener, this);
    wl_display_dispatch(display);
    wl_display_roundtrip(display);
    if (!inhibitor_manager)
    {
        std::cerr << "Compositor does not support "
                  << "wlr_input_inhibit_manager_v1" << std::endl;

        return false;
    }

    return true;
}

void WCM::create_main_layout()
{
    window.signal_key_press_event().connect([](GdkEventKey *event) {
        if (event->state & GDK_CONTROL_MASK && event->keyval == GDK_KEY_q)
            std::exit(0);
        return false;
    });

    main_page = std::make_unique<MainPage>(plugins);

    filter_label.property_margin().set_value(10);
    filter_label.set_markup("<span size=\"large\"><b>Filter</b></span>");
    main_left_panel_layout.pack_start(filter_label, false, false);

    search_entry.property_margin().set_value(10);
    search_entry.set_icon_from_icon_name("system-search", Gtk::ENTRY_ICON_SECONDARY);
    search_entry.signal_changed().connect([&] { main_page->set_filter(search_entry.get_text()); });
    search_entry.signal_key_press_event().connect([&](GdkEventKey *event) {
        if (event->keyval == GDK_KEY_Escape)
            search_entry.set_text("");
        return false;
    });
    main_left_panel_layout.pack_start(search_entry, false, false);

    close_button.property_margin().set_value(10);
    close_button.signal_clicked().connect([] { std::exit(0); });
    main_left_panel_layout.pack_end(close_button, false, false);

    output_config_button.property_margin().set_value(10);
    output_config_button.signal_clicked().connect([] { Glib::spawn_command_line_async(OUTPUT_CONFIG_PROGRAM); });
    main_left_panel_layout.pack_end(output_config_button, false, false);

    if (system("which " OUTPUT_CONFIG_PROGRAM " > /dev/null 2>&1") != 0)
    {
        output_config_button.set_sensitive(false);
        output_config_button.set_tooltip_markup("Cannot find program <tt>wdisplays</tt>");
    }

    plugin_left_panel_layout.pack_start(plugin_name_label, false, false);
    plugin_name_label.set_line_wrap();
    plugin_name_label.set_max_width_chars(15);
    plugin_name_label.set_alignment(Gtk::ALIGN_CENTER);
    plugin_name_label.property_margin().set_value(50);
    plugin_name_label.set_margin_bottom(25);
    plugin_left_panel_layout.pack_start(plugin_description_label, false, false);
    plugin_description_label.set_line_wrap();
    plugin_description_label.set_max_width_chars(20);
    plugin_description_label.set_margin_left(50);
    plugin_description_label.set_margin_right(50);
    plugin_left_panel_layout.pack_start(plugin_enabled_box, false, false);
    plugin_enabled_box.set_margin_top(25);
    plugin_enabled_box.pack_start(plugin_enabled_button);
    plugin_enabled_button.signal_toggled().connect(
        [=] { /* current_plugin->set_enabled(plugin_enabled_button.get_active()); */ });
    plugin_enabled_box.pack_start(plugin_enabled_label);
    plugin_enabled_box.set_halign(Gtk::ALIGN_CENTER);
    plugin_left_panel_layout.pack_end(back_button, false, false);
    back_button.property_margin().set_value(10);
    back_button.signal_clicked().connect([=] { open_page(); });

    left_stack.add(main_left_panel_layout);
    left_stack.add(plugin_left_panel_layout);
    left_stack.set_size_request(250, -1);
    left_stack.set_transition_type(Gtk::STACK_TRANSITION_TYPE_CROSSFADE);
    main_stack.add(*main_page);
    main_stack.set_transition_type(Gtk::STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);
    global_layout.pack_start(left_stack, false, false);
    global_layout.pack_start(main_stack, true, true);
    window.add(global_layout);
}

void WCM::open_page(Plugin *plugin)
{
    current_plugin = plugin;
    if (plugin)
    {
        plugin_enabled_box.set_visible(!plugin->is_core_plugin() && plugin->type != PLUGIN_TYPE_WF_SHELL);
        plugin_name_label.set_markup("<span size=\"large\"><b>" + plugin->disp_name + "</b></span>");
        plugin_description_label.set_markup("<span size=\"10000\"><b>" + plugin->tooltip + "</b></span>");
        plugin_page = std::make_unique<PluginPage>(plugin);
        main_stack.add(*plugin_page);
        plugin_page->show_all();
        main_stack.set_visible_child(*plugin_page);
        left_stack.set_visible_child(plugin_left_panel_layout);
    }
    else
    {
        main_stack.set_visible_child(*main_page);
        left_stack.set_visible_child(main_left_panel_layout);
    }
}

void WCM::load_config_files()
{
    wordexp_t exp;
    char *wf_config_file_override = getenv("WAYFIRE_CONFIG_FILE");
    char *wf_shell_config_file_override = getenv("WF_SHELL_CONFIG_FILE");

    if (wf_config_file_override)
    {
        wf_config_file = wf_config_file_override;
    }
    else
    {
        wordexp(WAYFIRE_CONFIG_FILE, &exp, 0);
        wf_config_file = exp.we_wordv[0];
        wordfree(&exp);
    }

    std::vector<std::string> wayfire_xmldirs;
    if (char *plugin_xml_path = getenv("WAYFIRE_PLUGIN_XML_PATH"))
    {
        std::stringstream ss(plugin_xml_path);
        std::string entry;
        while (std::getline(ss, entry, ':'))
        {
            wayfire_xmldirs.push_back(entry);
        }
    }

    wayfire_xmldirs.push_back(WAYFIRE_METADATADIR);

    wf_config_mgr =
        wf::config::build_configuration(wayfire_xmldirs, WAYFIRE_SYSCONFDIR "/wayfire/defaults.ini", wf_config_file);

    if (wf_shell_config_file_override)
    {
        wf_shell_config_file = wf_shell_config_file_override;
    }
    else
    {
        wordexp(WF_SHELL_CONFIG_FILE, &exp, 0);
        wf_shell_config_file = exp.we_wordv[0];
        wordfree(&exp);
    }

#if HAVE_WFSHELL
    std::vector<std::string> wf_shell_xmldirs(1, WFSHELL_METADATADIR);
    wf_shell_config_mgr = wf::config::build_configuration(
        wf_shell_xmldirs, WFSHELL_SYSCONFDIR "/wayfire/wf-shell-defaults.ini", wf_shell_config_file);
#endif
}

void WCM::save_to_file(wf::config::config_manager_t &mgr, const std::string &file)
{
    // TODO
    std::cout << "Saving to file " << file << std::endl;

    // wf::config::save_configuration_to_file(mgr, file);
}

bool WCM::save_config(Plugin *plugin)
{
    if (plugin->type == PLUGIN_TYPE_WAYFIRE)
    {
        save_to_file(wf_config_mgr, wf_config_file);
        return true;
    }
    if (plugin->type == PLUGIN_TYPE_WF_SHELL)
    {
        save_to_file(wf_shell_config_mgr, wf_shell_config_file);
        return true;
    }
    return false;
}

std::shared_ptr<wf::config::section_t> WCM::get_config_section(Plugin *plugin)
{
    if (plugin->type == PLUGIN_TYPE_WAYFIRE)
        return wf_config_mgr.get_section(plugin->name);
#ifdef HAVE_WFSHELL
    if (plugin->type == PLUGIN_TYPE_WF_SHELL)
        return wf_shell_config_mgr.get_section(plugin->name);
#endif
    return nullptr;
}

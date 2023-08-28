#include "wcm.hpp"
#include "utils.hpp"

#include <filesystem>
#include <libevdev/libevdev.h>
#include <wayfire/config/compound-option.hpp>
#include <wayfire/config/types.hpp>
#include <wayfire/config/xml.hpp>
#include <wordexp.h>

#define OUTPUT_CONFIG_PROGRAM "wdisplays"

constexpr int OPTION_LABEL_SIZE = 200;

bool KeyEntry::check_and_confirm(const std::string & key_str)
{
    if ((key_str.find_first_not_of(' ') != std::string::npos) && (key_str.front() != '<') &&
        ((key_str.find("BTN") != std::string::npos) ||
         (key_str.find("KEY") != std::string::npos)))
    {
        auto dialog = Gtk::MessageDialog("Attempting to bind <tt><b>\"" + key_str + '"' +
            "</b></tt> without modifier. You will be unable to use this key/button "
            "for anything else! Are you sure?",
            true, Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_YES_NO);
        return dialog.run() == Gtk::RESPONSE_YES;
    }

    return true;
}

mod_type KeyEntry::get_mod_from_keyval(guint keyval)
{
    if ((keyval == GDK_KEY_Shift_L) || (keyval == GDK_KEY_Shift_R))
    {
        return MOD_TYPE_SHIFT;
    }

    if ((keyval == GDK_KEY_Control_L) || (keyval == GDK_KEY_Control_R))
    {
        return MOD_TYPE_CONTROL;
    }

    if ((keyval == GDK_KEY_Alt_L) || (keyval == GDK_KEY_Alt_R) ||
        (keyval == GDK_KEY_Meta_L) || (keyval == GDK_KEY_Meta_R))
    {
        return MOD_TYPE_ALT;
    }

    if ((keyval == GDK_KEY_Super_L) || (keyval == GDK_KEY_Super_R))
    {
        return MOD_TYPE_SUPER;
    }

    return MOD_TYPE_NONE;
}

std::string KeyEntry::grab_key()
{
    static const auto HW_OFFSET = 8;

    auto grab_dialog = Gtk::Dialog("Waiting for Binding", true);
    auto label = Gtk::Label("none");
    grab_dialog.get_content_area()->pack_start(label);
    label.show();

    uint mod_mask = 0;
    std::string key_btn_string;

    auto cur_state_string = [&mod_mask]
    {
        std::string text;
        if (mod_mask & MOD_TYPE_SHIFT)
        {
            text += "<shift> ";
        }

        if (mod_mask & MOD_TYPE_CONTROL)
        {
            text += "<ctrl> ";
        }

        if (mod_mask & MOD_TYPE_ALT)
        {
            text += "<alt> ";
        }

        if (mod_mask & MOD_TYPE_SUPER)
        {
            text += "<super> ";
        }

        return text;
    };

    auto update_label = [=, &label]
    {
        const auto text = cur_state_string();
        label.set_text(text.empty() ? "(No modifiers pressed)" : text);
    };
    update_label();

    grab_dialog.signal_key_release_event().connect([&] (GdkEventKey *event)
    {
        mod_mask &= ~get_mod_from_keyval(event->keyval);
        update_label();
        return false;
    });
    grab_dialog.signal_key_press_event().connect(
        [&] (GdkEventKey *event)
    {
        auto new_mod = get_mod_from_keyval(event->keyval);
        mod_mask    |= new_mod;
        if (new_mod == MOD_TYPE_NONE)
        {
            key_btn_string =
                libevdev_event_code_get_name(EV_KEY,
                    event->hardware_keycode - HW_OFFSET);
            grab_dialog.response(Gtk::RESPONSE_ACCEPT);
        } else
        {
            update_label();
        }

        return true;
    },
        false);
    grab_dialog.signal_button_press_event().connect([&] (GdkEventButton *event)
    {
        key_btn_string.clear();
        switch (event->button)
        {
          case GDK_BUTTON_PRIMARY:
            key_btn_string = "BTN_LEFT";
            break;

          case GDK_BUTTON_MIDDLE:
            key_btn_string = "BTN_MIDDLE";
            break;

          case GDK_BUTTON_SECONDARY:
            key_btn_string = "BTN_RIGHT";
            break;

          case 4:
            key_btn_string = "BTN_SIDE";
            break;

          case 5:
            key_btn_string = "BTN_EXTRA";
            break;

          default:
            break;
        }

        grab_dialog.response(Gtk::RESPONSE_ACCEPT);
        return true;
    });

    if (!WCM::get_instance()->lock_input())
    {
        return "";
    }

    grab_dialog.fullscreen();
    auto result = grab_dialog.run();
    WCM::get_instance()->unlock_input();

    if (result == Gtk::RESPONSE_ACCEPT)
    {
        return cur_state_string() + key_btn_string;
    }

    return "";
}

KeyEntry::KeyEntry()
{
    add(grab_layout);
    add(edit_layout);
    set_transition_type(Gtk::STACK_TRANSITION_TYPE_CROSSFADE);

    grab_label.set_ellipsize(Pango::ELLIPSIZE_END);
    grab_button.add(grab_label);
    grab_button.signal_clicked().connect([=]
    {
        const auto value = grab_key();
        if (!value.empty() && check_and_confirm(value))
        {
            set_value(value);
        }
    });
    edit_button.set_image_from_icon_name("gtk-edit");
    edit_button.set_tooltip_text("Edit binding");
    edit_button.signal_clicked().connect([=]
    {
        entry.set_text(get_value());
        set_visible_child(edit_layout);
    });
    grab_layout.pack_start(grab_button, true, true);
    grab_layout.pack_start(edit_button, false, false);

    edit_layout.pack_start(entry, true, true);
    ok_button.set_image_from_icon_name("gtk-ok");
    ok_button.set_tooltip_text("Save binding");
    ok_button.signal_clicked().connect([=]
    {
        const std::string value = entry.get_text();
        if (check_and_confirm(value))
        {
            set_value(value);
            set_visible_child(grab_layout);
        }
    });
    cancel_button.set_image_from_icon_name("gtk-cancel");
    cancel_button.set_tooltip_text("Cancel");
    cancel_button.signal_clicked().connect([=] { set_visible_child(grab_layout); });
    edit_layout.pack_start(cancel_button, false, false);
    edit_layout.pack_start(ok_button, false, false);
}

static void add_comma_with_space(Glib::ustring& text)
{
    auto last_non_space = std::find_if_not(text.rbegin(), text.rend(), [] (char ch)
    {
        return std::isspace(ch);
    });
    if ((last_non_space != text.rend()) && (*last_non_space != ','))
    {
        text += ',';
    }

    if (!text.empty() && (*text.end() == ','))
    {
        text += ' ';
    }
}

LayoutsEntry::LayoutsEntry()
{
    for (const auto & [name, description] : get_xkb_layouts(WCM::get_instance()->get_xkb_rules()))
    {
        layouts.emplace_back(name + " — " + description);
        layouts.back().signal_activate().connect([name = name, this] ()
        {
            auto text = get_text();
            add_comma_with_space(text);
            set_text(text + name);
        });
    }

    set_tooltip_text("Open context menu to choose layout");

    signal_populate_popup().connect([&] (Gtk::Menu *menu)
    {
        menu->append(separator);
        for (auto & layout_item : layouts)
        {
            menu->append(layout_item);
        }

        menu->show_all();
    });
}

XkbModelEntry::XkbModelEntry()
{
    for (const auto & [name, description] : get_xkb_models(WCM::get_instance()->get_xkb_rules()))
    {
        models.emplace_back(name + " — " + description);
        models.back().signal_activate().connect([name = name, this] ()
        {
            set_text(name);
        });
    }

    set_tooltip_text("Open context menu to choose model");

    signal_populate_popup().connect([&] (Gtk::Menu *menu)
    {
        menu->append(separator);
        for (auto & model_item : models)
        {
            menu->append(model_item);
        }

        menu->show_all();
    });
}

std::ostream& operator <<(std::ostream & out, const wf::color_t & color)
{
    return out << "rgba(" << color.r << ", " << color.g << ", " << color.b << ", " <<
           color.a << ")";
}

template<class value_type>
void Option::set_value(wf_section section, const value_type & value)
{
    std::cout << section->get_name() << "." << name << " = " << value << "; ";
    section->get_option(name)->set_value_str(wf::option_type::to_string<value_type>(
        value));
    std::cout << section->get_option(name)->get_value_str() << "; ";
}

template<class... ArgTypes>
void Option::set_save(const ArgTypes &... args)
{
    auto section = WCM::get_instance()->get_config_section(plugin);
    if (!section)
    {
        return;
    }

    std::cout << __PRETTY_FUNCTION__ << ": \n  ";
    set_value(section, args...);
    WCM::get_instance()->save_config(plugin);
}

OptionWidget::OptionWidget(Option *option) : Gtk::Box(Gtk::ORIENTATION_HORIZONTAL,
        10)
{
    name_label.set_text(option->disp_name);
    name_label.set_tooltip_markup(option->tooltip);
    name_label.set_size_request(OPTION_LABEL_SIZE);
    name_label.set_alignment(Gtk::ALIGN_START);

    reset_button.set_image_from_icon_name("edit-clear");
    reset_button.set_tooltip_text("Reset to default");

    pack_start(name_label, false, false);
    Gtk::Box::pack_end(reset_button, false, false);

    auto section   = WCM::get_instance()->get_config_section(option->plugin);
    auto wf_option = section->get_option(option->name);

    switch (option->type)
    {
      case OPTION_TYPE_INT:
    {
        auto value_optional = wf::option_type::from_string<int>(
            wf_option->get_value_str());
        int value = value_optional ? value_optional.value() : std::get<int>(
            option->default_value);

        if (option->int_labels.empty())
        {
            auto spin_button = std::make_unique<Gtk::SpinButton>(
                Gtk::Adjustment::create(value, option->data.min, option->data.max,
                    1));
            spin_button->signal_value_changed().connect(
                [=, widget = spin_button.get()]
                {
                    option->set_save(widget->get_value_as_int());
                });
            reset_button.signal_clicked().connect(
                [=, widget = spin_button.get()]
                {
                    widget->set_value(std::get<int>(option->default_value));
                });
            pack_end(std::move(spin_button));
        } else
        {
            auto combo_box = std::make_unique<Gtk::ComboBoxText>();
            for (const auto & [name, _] : option->int_labels)
            {
                combo_box->append(name);
            }

            combo_box->set_active(value);
            combo_box->signal_changed().connect([=, widget = combo_box.get()]
                {
                    for (const auto & [name, int_value] : option->int_labels)
                    {
                        if (name == widget->get_active_text())
                        {
                            option->set_save(int_value);
                        }
                    }
                });
            reset_button.signal_clicked().connect(
                [=, widget = combo_box.get()]
                {
                    widget->set_active(std::get<int>(option->default_value));
                });
            pack_end(std::move(combo_box), true, true);
        }
    }
    break;

      case OPTION_TYPE_BOOL:
    {
        auto value_optional = wf::option_type::from_string<bool>(
            wf_option->get_value_str());
        bool value = value_optional ? value_optional.value() : std::get<int>(
            option->default_value);

        auto check_button = std::make_unique<Gtk::CheckButton>();
        check_button->set_active(value);
        check_button->signal_toggled().connect(
            [=, widget = check_button.get()]
            {
                option->set_save(widget->get_active());
            });
        reset_button.signal_clicked().connect(
            [=, widget = check_button.get()]
            {
                widget->set_active(std::get<int>(option->default_value));
            });
        pack_end(std::move(check_button));
    }
    break;

      case OPTION_TYPE_DOUBLE:
    {
        auto value_optional = wf::option_type::from_string<double>(
            wf_option->get_value_str());
        double value = value_optional ? value_optional.value() : std::get<double>(
            option->default_value);

        auto spin_box = std::make_unique<Gtk::SpinButton>(
            Gtk::Adjustment::create(value, option->data.min, option->data.max,
                option->data.precision),
            option->data.precision, 3);
        spin_box->signal_changed().connect([=, widget = spin_box.get()]
            {
                option->set_save(widget->get_value());
            });
        reset_button.signal_clicked().connect(
            [=, widget = spin_box.get()]
            {
                widget->set_value(std::get<double>(option->default_value));
            });
        pack_end(std::move(spin_box));
    }
    break;

      case OPTION_TYPE_ACTIVATOR:
      case OPTION_TYPE_BUTTON:
      case OPTION_TYPE_KEY:
    {
        auto key_entry = std::make_unique<KeyEntry>();
        key_entry->set_value(wf_option->get_value_str());
        key_entry->signal_changed().connect(
            [=, widget = key_entry.get()]
            {
                option->set_save(widget->get_value());
            });
        reset_button.signal_clicked().connect(
            [=, widget = key_entry.get()]
            {
                widget->set_value(std::get<std::string>(option->default_value));
            });
        pack_end(std::move(key_entry), true, true);
    }
    break;

      case OPTION_TYPE_GESTURE:
      case OPTION_TYPE_STRING:
    {
        if (option->str_labels.empty())
        {
            std::unique_ptr<Gtk::Entry> entry;
            if (option->name == "xkb_layout")
            {
                entry = std::make_unique<LayoutsEntry>();
            } else if (option->name == "xkb_model")
            {
                entry = std::make_unique<XkbModelEntry>();
            } else
            {
                entry = std::make_unique<Gtk::Entry>();
            }

            entry->set_text(wf_option->get_value_str());
            entry->signal_changed().connect(
                [=, widget = entry.get()]
                {
                    option->set_save<std::string>(widget->get_text());
                });

            auto run_dialog =
                [=, widget = entry.get()] (const Glib::ustring & label,
                                           Gtk::FileChooserAction action)
                {
                    auto dialog = Gtk::FileChooserDialog(label, action);
                    dialog.add_button("Cancel", Gtk::RESPONSE_CANCEL);
                    dialog.add_button("Open", Gtk::RESPONSE_ACCEPT);
                    if (dialog.run() == Gtk::RESPONSE_ACCEPT)
                    {
                        widget->set_text(dialog.get_filename());
                    }
                };
            if (option->data.hints & HINT_DIRECTORY)
            {
                auto dir_choose_button = std::make_unique<Gtk::Button>();
                dir_choose_button->set_image_from_icon_name("folder-open");
                dir_choose_button->set_tooltip_text("Choose Directory");
                dir_choose_button->signal_clicked().connect(
                    [=]
                    {
                        run_dialog("Select Folder",
                            Gtk::FILE_CHOOSER_ACTION_SELECT_FOLDER);
                    });
                pack_end(std::move(dir_choose_button));
            }

            if (option->data.hints & HINT_FILE)
            {
                auto file_choose_button = std::make_unique<Gtk::Button>();
                file_choose_button->set_image_from_icon_name("text-x-generic");
                file_choose_button->set_tooltip_text("Choose File");
                file_choose_button->signal_clicked().connect(
                    [=]
                    {
                        run_dialog("Select File", Gtk::FILE_CHOOSER_ACTION_OPEN);
                    });
                pack_end(std::move(file_choose_button));
            }

            reset_button.signal_clicked().connect(
                [=, widget = entry.get()]
                {
                    widget->set_text(std::get<std::string>(option->default_value));
                });
            pack_end(std::move(entry), true, true);
        } else
        {
            auto combo_box = std::make_unique<Gtk::ComboBoxText>();
            for (const auto & [name, str_value] : option->str_labels)
            {
                combo_box->append(str_value, name);
                if (str_value == wf_option->get_value_str())
                {
                    combo_box->set_active_id(str_value);
                }
            }

            combo_box->signal_changed().connect(
                [=, widget = combo_box.get()]
                {
                    option->set_save<std::string>(widget->get_active_id());
                });
            reset_button.signal_clicked().connect(
                [=, widget = combo_box.get()]
                {
                    widget->set_active_id(wf_option->get_default_value_str());
                });
            pack_end(std::move(combo_box), true, true);
        }
    }
    break;

      case OPTION_TYPE_COLOR:
    {
        auto value_optional = wf::option_type::from_string<wf::color_t>(
            wf_option->get_value_str());
        wf::color_t value =
            value_optional ?
            value_optional.value() :
            wf::option_type::from_string<wf::color_t>(std::get<std::string>(option->
                default_value)).value();

        Gdk::RGBA rgba;
        rgba.set_rgba(value.r, value.g, value.b, value.a);
        auto color_button = std::make_unique<Gtk::ColorButton>(rgba);
        color_button->set_use_alpha(true);
        color_button->set_title(option->disp_name);
        color_button->property_rgba().signal_changed().connect([=,
                                                                widget =
                                                                    color_button.get()]
            {
                auto rgba = widget->get_rgba();
                wf::color_t color = {rgba.get_red(), rgba.get_green(),
                    rgba.get_blue(), rgba.get_alpha()};
                option->set_save(color);
            });
        reset_button.signal_clicked().connect([=, widget = color_button.get()]
            {
                auto color =
                    wf::option_type::from_string<wf::color_t>(std::get<std::string>(
                        option->default_value)).value();
                Gdk::RGBA rgba;
                rgba.set_rgba(color.r, color.g, color.b, color.a);
                widget->set_rgba(rgba);
            });
        pack_end(std::move(color_button));
    }
    break;

      default:
        break;
    }
}

AutostartDynamicList::AutostartWidget::AutostartWidget(Option *option) : Gtk::Box(
        Gtk::ORIENTATION_HORIZONTAL, 10)
{
    command_entry.set_text(std::get<std::string>(option->default_value));
    command_entry.signal_changed().connect([=]
    {
        option->set_save<std::string>(command_entry.get_text());
    });
    choose_button.set_image_from_icon_name("application-x-executable");
    choose_button.set_tooltip_text("Choose Executable");
    choose_button.signal_clicked().connect([&]
    {
        auto dialog = Gtk::FileChooserDialog("Choose Executable");
        dialog.add_button("Cancel", Gtk::RESPONSE_CANCEL);
        dialog.add_button("Open", Gtk::RESPONSE_ACCEPT);
        if (dialog.run() == Gtk::RESPONSE_ACCEPT)
        {
            command_entry.set_text(dialog.get_filename());
        }
    });
    run_button.set_image_from_icon_name("media-playback-start");
    run_button.set_tooltip_text("Run command");
    run_button.signal_clicked().connect([=]
    {
        Glib::spawn_command_line_async(command_entry.get_text());
    });
    remove_button.set_image_from_icon_name("list-remove");
    remove_button.set_tooltip_text("Remove from autostart list");
    remove_button.signal_clicked().connect([=]
    {
        auto section = WCM::get_instance()->get_config_section(option->plugin);
        section->unregister_option(section->get_option(option->name));
        WCM::get_instance()->save_config(option->plugin);
        delete option;
        ((AutostartDynamicList*)get_parent())->remove(this);
    });
    pack_start(command_entry, true, true);
    pack_start(choose_button, false, false);
    pack_start(run_button, false, false);
    pack_start(remove_button, false, false);
}

BindingsDynamicList::BindingWidget::BindingWidget(const std::string & cmd_name,
    Option *option, wf_section section)
{
    const auto command = "command_" + cmd_name;
    const auto regular_binding_name = "binding_" + cmd_name;
    const auto repeat_binding_name  = "repeatable_binding_" + cmd_name;
    const auto always_binding_name  = "always_binding_" + cmd_name;

    auto executable_opt = section->get_option(command);
    auto regular_opt    = section->get_option_or(regular_binding_name);
    auto repeatable_opt = section->get_option_or(repeat_binding_name);
    auto always_opt     = section->get_option_or(always_binding_name);
    auto [wf_opt,
          opt_name] = always_opt ? std::tie(always_opt, always_binding_name) :
        repeatable_opt ? std::tie(repeatable_opt, repeat_binding_name) :
        std::tie(regular_opt, regular_binding_name);
    binding_wf_opt = wf_opt;
    if (!binding_wf_opt)
    {
        binding_wf_opt = std::make_shared<wf::config::option_t<std::string>>(
            regular_binding_name, "none");
    }

    add(expander);
    expander.add(vbox);
    if (executable_opt->get_value_str().empty())
    {
        expander.set_expanded();
    }

    vbox.property_margin().set_value(5);

    Option *key_option =
        option->create_child_option(opt_name, OPTION_TYPE_ACTIVATOR);
    key_entry = std::make_unique<KeyEntry>();
    key_entry->set_value(binding_wf_opt->get_value_str());
    key_entry->signal_changed().connect([=]
    {
        key_option->set_save(key_entry->get_value());
    });

    Option *command_option =
        option->create_child_option(command, OPTION_TYPE_STRING);

    type_label.set_size_request(OPTION_LABEL_SIZE);
    type_label.set_alignment(Gtk::ALIGN_START);
    type_box.pack_start(type_label, false, false);
    type_combo_box.append("Regular");
    type_combo_box.append("Repeat");
    type_combo_box.append("Always");
    type_combo_box.set_active(always_opt ? 2 : repeatable_opt ? 1 : 0);
    type_combo_box.signal_changed().connect([=]
    {
        section->unregister_option(binding_wf_opt);
        auto type = type_combo_box.get_active_row_number();
        key_option->name = type == 2 ? always_binding_name : type == 1 ? repeat_binding_name : regular_binding_name;
        binding_wf_opt   =
            std::make_shared<wf::config::option_t<std::string>>(key_option->name,
                binding_wf_opt->get_value_str());
        section->register_new_option(binding_wf_opt);
        WCM::get_instance()->save_config(option->plugin);
    });
    type_box.pack_start(type_combo_box, true, true);
    vbox.pack_start(type_box, false, false);

    binding_label.set_size_request(OPTION_LABEL_SIZE);
    binding_label.set_alignment(Gtk::ALIGN_START);
    binding_box.pack_start(binding_label, false, false);
    binding_box.pack_start(*key_entry, true, true);
    vbox.pack_start(binding_box, false, false);

    command_label.set_size_request(OPTION_LABEL_SIZE);
    command_label.set_alignment(Gtk::ALIGN_START);
    command_box.pack_start(command_label, false, false);
    expander.set_label("Command " + cmd_name);
    command_entry.signal_changed().connect([=]
    {
        expander.set_label("Command " + cmd_name + ": " + command_entry.get_text());
        auto *label = (Gtk::Label*)expander.get_label_widget();
        label->set_ellipsize(Pango::ELLIPSIZE_END);
        label->set_tooltip_text(command_entry.get_text());
    });
    command_entry.set_text(executable_opt->get_value_str());
    command_entry.signal_changed().connect([=]
    {
        command_option->set_save<std::string>(command_entry.get_text());
    });
    command_box.pack_start(command_entry, true, true);
    remove_button.set_image_from_icon_name("list-remove");
    remove_button.signal_clicked().connect([=]
    {
        ((BindingsDynamicList*)get_parent())->remove(this);
        section->unregister_option(always_opt);
        section->unregister_option(repeatable_opt);
        section->unregister_option(regular_opt);
        section->unregister_option(executable_opt);
        WCM::get_instance()->save_config(option->plugin);
    });
    command_box.pack_start(remove_button, false, false);
    vbox.pack_start(command_box);
}

template<enum VswitchBindingKind kind>
VswitchBindingsDynamicList<kind>::BindingWidget::BindingWidget(std::shared_ptr<wf::config::section_t> section,
    Option *option, int workspace_index) : Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 10)
{
    Option *key_option = option->create_child_option(OPTION_PREFIX + std::to_string(
        workspace_index), OPTION_TYPE_KEY);
    binding_wf_opt = section->get_option(key_option->name);
    key_entry.set_value(binding_wf_opt->get_value_str());
    key_entry.signal_changed().connect([=]
    {
        key_option->set_save(key_entry.get_value());
    });
    label.set_alignment(Gtk::ALIGN_START);
    remove_button.set_image_from_icon_name("list-remove");
    remove_button.signal_clicked().connect([=] ()
    {
        ((VswitchBindingsDynamicList<kind>*)get_parent())->remove(this);
        section->unregister_option(binding_wf_opt);
        WCM::get_instance()->save_config(option->plugin);
    });

    workspace_spin_button.set_tooltip_text("Workspace Index");
    workspace_spin_button.set_value(workspace_index);
    workspace_spin_button.signal_value_changed().connect([=]
    {
        section->unregister_option(binding_wf_opt);
        key_option->name = OPTION_PREFIX + std::to_string(workspace_spin_button.get_value_as_int());
        binding_wf_opt   =
            std::make_shared<wf::config::option_t<std::string>>(key_option->name,
                binding_wf_opt->get_value_str());
        section->register_new_option(binding_wf_opt);
        WCM::get_instance()->save_config(option->plugin);
    });

    pack_start(label, false, false);
    pack_start(workspace_spin_button, false, false);
    pack_end(remove_button, false, false);
    pack_end(key_entry);
}

DynamicListBase::DynamicListBase() : Gtk::Box(Gtk::ORIENTATION_VERTICAL, 10)
{
    add_button.set_image_from_icon_name("list-add");
    add_box.pack_end(add_button, false, false);
    pack_end(add_box, false, false);
}

AutostartDynamicList::AutostartDynamicList(Option *option)
{
    std::shared_ptr<wf::config::section_t> section =
        WCM::get_instance()->get_config_section(option->plugin);
    if (!section)
    {
        return;
    }

    auto wf_option = std::dynamic_pointer_cast<wf::config::compound_option_t>(section->get_option(
        "autostart"));
    auto autostart_names = wf_option->get_value<std::string>();
    option->options.clear();

    for (const auto & [opt_name, executable] : autostart_names)
    {
        if (std::get<std::string>(option->default_value) != "string")
        {
            continue;
        }

        Option *dyn_opt = option->create_child_option(opt_name, OPTION_TYPE_STRING);
        dyn_opt->default_value = executable;

        pack_widget(std::make_unique<AutostartWidget>(dyn_opt));
    }

    add_button.set_tooltip_text("Add new command");
    add_button.signal_clicked().connect([=]
    {
        static const std::string prefix = "autostart";
        int i = 0;
        while (section->get_option_or(prefix + std::to_string(i)))
        {
            ++i;
        }

        const auto name = prefix + std::to_string(i);
        const std::string executable = "<command>";
        section->register_new_option(std::make_shared<wf::config::option_t<std::string>>(
            name, executable));
        WCM::get_instance()->save_config(option->plugin);
        Option *dyn_opt = option->create_child_option(name, OPTION_TYPE_STRING);
        dyn_opt->default_value = executable;
        pack_widget(std::make_unique<AutostartWidget>(dyn_opt));
        show_all();
    });
}

BindingsDynamicList::BindingsDynamicList(Option *option)
{
    std::shared_ptr<wf::config::section_t> section =
        WCM::get_instance()->get_config_section(option->plugin);
    if (!section)
    {
        return;
    }

    std::vector<std::string> command_names;
    static const std::string exec_prefix = "command_";
    for (const auto & command : section->get_registered_options())
    {
        if (begins_with(command->get_name(), exec_prefix))
        {
            command_names.push_back(command->get_name().substr(exec_prefix.length()));
        }
    }

    option->options.clear();
    for (const auto & cmd_name : command_names)
    {
        pack_widget(std::make_unique<BindingWidget>(cmd_name, option, section));
    }

    add_button.signal_clicked().connect([=]
    {
        int i = 0;
        while (section->get_option_or(exec_prefix + std::to_string(i)))
        {
            ++i;
        }

        const auto cmd_name = std::to_string(i);
        section->register_new_option(std::make_shared<wf::config::option_t<std::string>>(
            exec_prefix + cmd_name, ""));
        section->register_new_option(
            std::make_shared<wf::config::option_t<std::string>>("binding_" +
                cmd_name, "none"));
        WCM::get_instance()->save_config(option->plugin);
        pack_widget(std::make_unique<BindingWidget>(cmd_name, option, section));
        show_all();
    });
}

template<>
const std::string VswitchBindingsDynamicList<VswitchBindingKind::WITHOUT_WINDOW>::OPTION_PREFIX = "binding_";
template<>
const std::string VswitchBindingsDynamicList<VswitchBindingKind::WITH_WINDOW>::OPTION_PREFIX = "with_win_";
template<>
const std::string VswitchBindingsDynamicList<VswitchBindingKind::SEND_WINDOW>::OPTION_PREFIX = "send_win_";
template<>
const Glib::ustring VswitchBindingsWidget<VswitchBindingKind::WITHOUT_WINDOW>::LABEL_TEXT = "Go To Workspace";
template<>
const Glib::ustring VswitchBindingsWidget<VswitchBindingKind::WITH_WINDOW>::LABEL_TEXT =
    "Go To Workspace With Window";
template<>
const Glib::ustring VswitchBindingsWidget<VswitchBindingKind::SEND_WINDOW>::LABEL_TEXT =
    "Send Window To Workspace";

template<enum VswitchBindingKind kind>
VswitchBindingsDynamicList<kind>::VswitchBindingsDynamicList(Option *option)
{
    auto section = WCM::get_instance()->get_config_section(option->plugin);
    option->options.clear();

    for (auto vswitch_option : section->get_registered_options())
    {
        if (begins_with(vswitch_option->get_name(), OPTION_PREFIX))
        {
            try {
                std::cerr << vswitch_option->get_name().substr(OPTION_PREFIX.size()) << std::endl;
                int workspace_index = std::stoi(vswitch_option->get_name().substr(OPTION_PREFIX.size()));
                pack_widget(std::make_unique<BindingWidget>(section, option, workspace_index));
            } catch (const std::invalid_argument& e)
            {
                std::cerr << "Invalid workspace index: " << e.what() << std::endl;
            }
        }
    }

    add_button.signal_clicked().connect([=]
    {
        int workspace_index = 1;
        while (section->get_option_or(OPTION_PREFIX + std::to_string(workspace_index)))
        {
            ++workspace_index;
        }

        Option *binding_option =
            option->create_child_option(OPTION_PREFIX + std::to_string(workspace_index), OPTION_TYPE_STRING);
        section->register_new_option(std::make_shared<wf::config::option_t<std::string>>(binding_option->name,
            ""));
        pack_widget(std::make_unique<BindingWidget>(section, binding_option, workspace_index));
        show_all();
        WCM::get_instance()->save_config(option->plugin);
    });
}

template<enum VswitchBindingKind kind>
VswitchBindingsWidget<kind>::VswitchBindingsWidget(Option *option) : dynamic_list(option)
{
    set_label(LABEL_TEXT + " Bindings");
    dynamic_list.property_margin().set_value(5);
    add(dynamic_list);
}

OptionSubgroupWidget::OptionSubgroupWidget(Option *subgroup)
{
    add(expander);
    expander.set_label(subgroup->name);
    expander.add(expander_layout);
    for (Option *option : subgroup->options)
    {
        option_widgets.emplace_back(option);
        expander_layout.pack_start(option_widgets.back());
    }
}

OptionGroupWidget::OptionGroupWidget(Option *group)
{
    add(options_layout);
    options_layout.property_margin().set_value(10);
    set_vexpand();

    for (Option *option : group->options)
    {
        if (option->hidden)
        {
            continue;
        }

        bool fill_expand = false;
        if ((option->type == OPTION_TYPE_SUBGROUP) && !option->options.empty())
        {
            option_widgets.push_back(std::make_unique<OptionSubgroupWidget>(option));
        } else if (option->type == OPTION_TYPE_DYNAMIC_LIST)
        {
            std::cout << option->name << std::endl;
            if (option->name == "autostart")
            {
                option_widgets.push_back(std::make_unique<AutostartDynamicList>(
                    option));
            } else if (option->name == "bindings")
            {
                option_widgets.push_back(std::make_unique<BindingsDynamicList>(
                    option));
            } else if (option->name == "workspace_bindings")
            {
                option_widgets.push_back(std::make_unique<VswitchBindingsWidget<VswitchBindingKind::WITHOUT_WINDOW>>(
                    option));
            } else if (option->name == "workspace_bindings_win")
            {
                option_widgets.push_back(std::make_unique<VswitchBindingsWidget<VswitchBindingKind::WITH_WINDOW>>(
                    option));
            } else if (option->name == "bindings_win")
            {
                option_widgets.push_back(std::make_unique<VswitchBindingsWidget<VswitchBindingKind::SEND_WINDOW>>(
                    option));
            }
            // TODO other dynamic lists
            else
            {
                continue;
            }

            fill_expand = true;
        } else
        {
            option_widgets.push_back(std::make_unique<OptionWidget>(option));
        }

        options_layout.pack_start(*option_widgets.back(), fill_expand, fill_expand);
    }
}

PluginPage::PluginPage(Plugin *plugin)
{
    set_scrollable();
    for (auto *group : plugin->option_groups)
    {
        if ((group->type != OPTION_TYPE_GROUP) || group->hidden)
        {
            continue;
        }

        groups.emplace_back(group);
        append_page(groups.back(), group->name);
    }
}

MainPage::Category::Category(const Glib::ustring & name,
    const Glib::ustring & icon_name) : name(name)
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
    flowbox.set_halign(Gtk::ALIGN_START);
    flowbox.set_min_children_per_line(3);
}

void Plugin::init_widget()
{
    label.set_text(disp_name);
    label.set_ellipsize(Pango::ELLIPSIZE_END);
    icon.set(WCM::get_instance()->find_icon("plugin-" + name + ".svg"));
    button_layout.pack_start(icon);
    button_layout.pack_start(label);
    button_layout.set_halign(Gtk::ALIGN_START);
    button.set_tooltip_markup(tooltip);
    button.set_relief(Gtk::RELIEF_NONE);
    button.add(button_layout);
    enabled_check.set_active(enabled);
    widget.set_halign(Gtk::ALIGN_START);
    widget.pack_start(enabled_check, false, false);
    if (!is_core_plugin() && (type == PLUGIN_TYPE_WAYFIRE))
    {
        enabled_check.signal_toggled().connect(
            [=]
        {
            WCM::get_instance()->set_plugin_enabled(this,
                enabled_check.get_active());
        });
    } else
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

MainPage::MainPage(const std::vector<Plugin*> & plugins) : plugins(plugins)
{
    add(vbox);
    for (auto *plugin : plugins)
    {
        std::find_if(categories.begin(), categories.end() - 1,
            [=] (const Category & cat)
        {
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

void MainPage::set_filter(const Glib::ustring & filter)
{
    std::map<std::string, bool> category_visible;
    for (const auto & cat : categories)
    {
        category_visible.insert({cat.name, false});
    }

    for (auto *plug : plugins)
    {
        bool plug_visible = find_string(plug->name, filter) || find_string(
            plug->disp_name, filter) ||
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

void WCM::parse_config(wf::config::config_manager_t & config_manager)
{
    for (auto & s : config_manager.get_all_sections())
    {
        xmlNode *root_element = wf::config::xml::get_section_xml_node(s);

        if (!root_element)
        {
            continue;
        }

        root_element = root_element->parent;
        std::string root_name = (char*)root_element->name;

        if ((root_element->type == XML_ELEMENT_NODE) &&
            ((root_name == "wayfire") || (root_name == "wf-shell")))
        {
            printf("Loading %s plugin: %s\n", root_element->name,
                s->get_name().c_str());
            Plugin *p = Plugin::get_plugin_data(root_element);
            if (p)
            {
                plugins.push_back(p);
            } else
            {
                continue;
            }

            if (root_name == "wayfire")
            {
                p->type = PLUGIN_TYPE_WAYFIRE;
            } else if (root_name == "wf-shell")
            {
                p->type = PLUGIN_TYPE_WF_SHELL;
            } else
            {
                // Should be unreachable because `root_name` is "wayfire" or
                // "wf-shell"
                p->type = PLUGIN_TYPE_NONE;
            }
        }
    }
}

std::string::size_type find_plugin(Plugin *p, const std::string & plugins)
{
    std::string::size_type pos = 0;
    while (true)
    {
        pos = plugins.find(p->name, pos);

        if (pos == std::string::npos)
        {
            break;
        }

        if (((pos == 0) || (plugins[pos - 1] == ' ')) &&
            ((pos + p->name.length() == plugins.length()) ||
             (plugins[pos + p->name.length()] == ' ')))
        {
            return pos;
        }

        pos += p->name.length();
    }

    return std::string::npos;
}

bool plugin_enabled(Plugin *p, const std::string & plugins)
{
    return p->is_core_plugin() || p->type == PLUGIN_TYPE_WF_SHELL || find_plugin(p,
        plugins) !=
           std::string::npos;
}

WCM::WCM(Glib::RefPtr<Gtk::Application> app)
{
    if (instance)
    {
        throw std::logic_error("WCM should not be initialized more than once");
    }

    instance = this;

    app->add_main_option_entry([this] (const Glib::ustring &, const Glib::ustring & value, bool)
    {
        wf_config_file = value;
        return true;
    }, "config", 'c', "Wayfire config file to use", "file");
    app->add_main_option_entry([this] (const Glib::ustring &, const Glib::ustring & value, bool)
    {
        wf_shell_config_file = value;
        return true;
    }, "shell-config", 's', "wf-shell config file to use", "file");

    app->signal_startup().connect([this, app] ()
    {
        load_config_files();
        parse_config();
#if HAVE_WFSHELL
        parse_wfshell_config();
#endif

        if (!init_input_inhibitor())
        {
            std::cerr << "Binding grabs will not work" << std::endl;
        }

        const auto plugins_str =
            wf_config_mgr.get_section("core")->get_option("plugins")->get_value_str();
        for (auto *plugin : plugins)
        {
            plugin->enabled = plugin_enabled(plugin, plugins_str);
        }

        window    = std::make_unique<Gtk::ApplicationWindow>(app);
        auto icon = Gdk::Pixbuf::create_from_file(find_icon("wcm.png"));
        window->set_icon(icon);
        window->set_size_request(750, 550);
        window->set_default_size(1000, 580);
        window->set_title("Wayfire Config Manager");
        create_main_layout();
        window->show_all();
    });

    app->signal_activate().connect([&] { window->present(); });
}

static void registry_add_object(void *data, struct wl_registry *registry,
    uint32_t name, const char *interface,
    uint32_t version)
{
    WCM *wcm = (WCM*)data;

    if (strcmp(interface, zwlr_input_inhibit_manager_v1_interface.name) == 0)
    {
        wcm->set_inhibitor_manager((zwlr_input_inhibit_manager_v1*)wl_registry_bind(
            registry, name, &zwlr_input_inhibit_manager_v1_interface, 1u));
    }
}

static void registry_remove_object(void *data, struct wl_registry *registry,
    uint32_t name)
{}

static struct wl_registry_listener registry_listener = {
    &registry_add_object, &registry_remove_object
};

bool WCM::init_input_inhibitor()
{
    struct wl_display *display = gdk_wayland_display_get_wl_display(
        gdk_display_get_default());
    if (!display)
    {
        std::cerr << "Failed to acquire wl_display for input inhibitor" << std::endl;

        return false;
    }

    struct wl_registry *registry = wl_display_get_registry(display);
    if (!registry)
    {
        std::cerr << "Failed to acquire wl_registry for input inhibitor" <<
            std::endl;

        return false;
    }

    wl_registry_add_listener(registry, &registry_listener, this);
    wl_display_dispatch(display);
    wl_display_roundtrip(display);
    if (!inhibitor_manager)
    {
        std::cerr << "Compositor does not support " <<
            "wlr_input_inhibit_manager_v1" << std::endl;

        return false;
    }

    return true;
}

bool WCM::lock_input()
{
    if (!inhibitor_manager)
    {
        std::cerr << "Compositor does not support wlr_input_inhibit_manager_v1!" <<
            std::endl;

        return false;
    }

    if (screen_lock)
    {
        return false;
    }

    /* Lock input */
    screen_lock = zwlr_input_inhibit_manager_v1_get_inhibitor(inhibitor_manager);

    return true;
}

void WCM::unlock_input()
{
    if (!screen_lock)
    {
        return;
    }

    zwlr_input_inhibitor_v1_destroy(screen_lock);
    wl_display_flush(gdk_wayland_display_get_wl_display(gdk_display_get_default()));
    screen_lock = nullptr;
}

void WCM::set_plugin_enabled(Plugin *plugin, bool enabled)
{
    if (!plugin || plugin->is_core_plugin() ||
        (plugin->type == PLUGIN_TYPE_WF_SHELL))
    {
        return;
    }

    plugin->enabled = enabled;
    plugin->enabled_check.set_active(enabled);
    auto wf_opt = wf_config_mgr.get_section("core")->get_option("plugins");
    std::string enabled_plugins = wf_opt->get_value_str();

    if (!enabled)
    {
        auto pos = find_plugin(plugin, enabled_plugins);
        if (pos == std::string::npos)
        {
            return;
        }

        while (pos != std::string::npos)
        {
            enabled_plugins.erase(
                pos - ( /* remove space before */ pos != 0 ? 1 : 0),
                plugin->name.length() +
                (enabled_plugins.length() != plugin->name.length() ? 1 : 0));
            pos = find_plugin(plugin, enabled_plugins);
        }
    } else
    {
        if (find_plugin(plugin, enabled_plugins) != std::string::npos)
        {
            return;
        }

        enabled_plugins.append((enabled_plugins.empty() ? "" : " ") + plugin->name);
    }

    wf_opt->set_value_str(enabled_plugins);
    save_to_file(wf_config_mgr, wf_config_file);
}

void WCM::create_main_layout()
{
    window->signal_key_press_event().connect([] (GdkEventKey *event)
    {
        if (event->state & GDK_CONTROL_MASK && (event->keyval == GDK_KEY_q))
        {
            std::exit(0);
        }

        return false;
    });

    main_page = std::make_unique<MainPage>(plugins);

    filter_label.property_margin().set_value(10);
    filter_label.set_markup("<span size=\"large\"><b>Filter</b></span>");
    main_left_panel_layout.pack_start(filter_label, false, false);

    search_entry.property_margin().set_value(10);
    search_entry.signal_search_changed().connect([&]
    {
        main_page->set_filter(search_entry.get_text());
    });
    search_entry.signal_key_press_event().connect([&] (GdkEventKey *event)
    {
        if (event->keyval == GDK_KEY_Escape)
        {
            search_entry.set_text("");
        }

        return false;
    });
    main_left_panel_layout.pack_start(search_entry, false, false);

    close_button.property_margin().set_value(10);
    close_button.signal_clicked().connect([] { std::exit(0); });
    main_left_panel_layout.pack_end(close_button, false, false);

    output_config_button.property_margin().set_value(10);
    output_config_button.signal_clicked().connect([]
    {
        Glib::spawn_command_line_async(OUTPUT_CONFIG_PROGRAM);
    });
    main_left_panel_layout.pack_end(output_config_button, false, false);

    if (system("which " OUTPUT_CONFIG_PROGRAM " > /dev/null 2>&1") != 0)
    {
        output_config_button.set_sensitive(false);
        output_config_button.set_tooltip_markup(
            "Cannot find program <tt>wdisplays</tt>");
    }

    plugin_left_panel_layout.pack_start(plugin_name_label, false, false);
    plugin_name_label.set_line_wrap();
    plugin_name_label.set_max_width_chars(15);
    plugin_name_label.set_alignment(Gtk::ALIGN_CENTER);
    plugin_name_label.set_justify(Gtk::JUSTIFY_CENTER);
    plugin_name_label.property_margin().set_value(50);
    plugin_name_label.set_margin_bottom(25);
    plugin_left_panel_layout.pack_start(plugin_description_label, false, false);
    plugin_description_label.set_line_wrap();
    plugin_description_label.set_max_width_chars(20);
    plugin_description_label.set_alignment(Gtk::ALIGN_CENTER);
    plugin_description_label.set_justify(Gtk::JUSTIFY_CENTER);
    plugin_description_label.set_margin_left(50);
    plugin_description_label.set_margin_right(50);
    plugin_left_panel_layout.pack_start(plugin_enabled_box, false, false);
    plugin_enabled_box.set_margin_top(25);
    plugin_enabled_box.pack_start(plugin_enabled_check);
    plugin_enabled_check.signal_toggled().connect([=]
    {
        if (current_plugin)
        {
            set_plugin_enabled(current_plugin, plugin_enabled_check.get_active());
        }
    });
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
    window->add(global_layout);
}

void WCM::open_page(Plugin *plugin)
{
    if (plugin)
    {
        plugin_enabled_box.set_visible(
            !plugin->is_core_plugin() && plugin->type != PLUGIN_TYPE_WF_SHELL);
        plugin_enabled_check.set_active(plugin->enabled);
        plugin_name_label.set_markup(
            "<span size=\"12000\"><b>" + plugin->disp_name + "</b></span>");
        plugin_description_label.set_markup(
            "<span size=\"10000\"><b>" + plugin->tooltip + "</b></span>");
        plugin_page = std::make_unique<PluginPage>(plugin);
        main_stack.add(*plugin_page);
        plugin_page->show_all();
        main_stack.set_visible_child(*plugin_page);
        left_stack.set_visible_child(plugin_left_panel_layout);
    } else
    {
        main_stack.set_visible_child(*main_page);
        left_stack.set_visible_child(main_left_panel_layout);
    }

    current_plugin = plugin;
}

static std::string wordexp_str(const char *str)
{
    wordexp_t exp;
    wordexp(str, &exp, 0);
    std::string result = exp.we_wordv[0];
    wordfree(&exp);
    return result;
}

void WCM::load_config_files()
{
    const char *wf_config_file_override = getenv("WAYFIRE_CONFIG_FILE");
    const char *wf_shell_config_file_override = getenv("WF_SHELL_CONFIG_FILE");

    if (wf_config_file.empty())
    {
        wf_config_file = wordexp_str(wf_config_file_override ? wf_config_file_override : WAYFIRE_CONFIG_FILE);
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
        wf::config::build_configuration(wayfire_xmldirs,
            WAYFIRE_SYSCONFDIR "/wayfire/defaults.ini",
            wf_config_file);

    if (wf_shell_config_file.empty())
    {
        wf_shell_config_file = wordexp_str(
            wf_shell_config_file_override ? wf_shell_config_file_override : WF_SHELL_CONFIG_FILE);
    }

#if HAVE_WFSHELL
    std::vector<std::string> wf_shell_xmldirs(1, WFSHELL_METADATADIR);
    wf_shell_config_mgr = wf::config::build_configuration(
        wf_shell_xmldirs, WFSHELL_SYSCONFDIR "/wayfire/wf-shell-defaults.ini",
        wf_shell_config_file);
#endif
}

/**
 * Adapted from wf-config internal source code.
 *
 * Go through all options in the section, try to match them against the prefix
 * of the compound option, thus build a new value and set it.
 */
void update_compound_from_section(wf::config::compound_option_t *compound,
    const std::shared_ptr<wf::config::section_t> & section)
{
    auto options = section->get_registered_options();
    std::vector<std::vector<std::string>> new_value;

    struct tuple_in_construction_t
    {
        // How many of the tuple elements were initialized
        size_t initialized = 0;
        std::vector<std::string> values;
    };

    std::map<std::string, std::vector<std::string>> new_values;
    const auto & entries = compound->get_entries();

    for (size_t n = 0; n < entries.size(); n++)
    {
        const auto & prefix = entries[n]->get_prefix();
        for (auto & opt : options)
        {
            if (wf::config::xml::get_option_xml_node(opt))
            {
                continue;
            }

            if (begins_with(opt->get_name(), prefix))
            {
                // We have found a match.
                // Find the suffix we should store values in.
                std::string suffix = opt->get_name().substr(prefix.size());
                if (!new_values.count(suffix) && (n > 0))
                {
                    // Skip entries which did not have their first value set,
                    // because these will not be fully constructed in the end.
                    continue;
                }

                auto & tuple = new_values[suffix];

                // Parse the value from the option, with the n-th type.
                if (!entries[n]->is_parsable(opt->get_value_str()))
                {
                    continue;
                }

                if (n == 0)
                {
                    // Push the suffix first
                    tuple.push_back(suffix);
                }

                // Update the Nth entry in the tuple (+1 because the first entry
                // is the amount of initialized entries).
                tuple.push_back(opt->get_value_str());
            }
        }
    }

    wf::config::compound_option_t::stored_type_t value;
    for (auto & e : new_values)
    {
        // Ignore entires which do not have all entries set
        if (e.second.size() != entries.size() + 1)
        {
            continue;
        }

        value.push_back(std::move(e.second));
    }

    compound->set_value_untyped(value);
}

/**
 * Save the given configuration to the given file.
 *
 * Update the values of the compound options while doing this, as if
 * they were set from the config file. This is necessary because wf-config will only
 * save values in the compound list itself, not the options which represent the
 * entries in the list.
 */
void WCM::save_to_file(wf::config::config_manager_t & mgr, const std::string & file)
{
    std::cout << "Saving to file " << file << std::endl;

    for (auto & section : mgr.get_all_sections())
    {
        for (auto & opt : section->get_registered_options())
        {
            auto as_compound =
                dynamic_cast<wf::config::compound_option_t*>(opt.get());
            if (as_compound)
            {
                update_compound_from_section(as_compound, section);
            }
        }
    }

    wf::config::save_configuration_to_file(mgr, file);
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
    {
        return wf_config_mgr.get_section(plugin->name);
    }

#ifdef HAVE_WFSHELL
    if (plugin->type == PLUGIN_TYPE_WF_SHELL)
    {
        return wf_shell_config_mgr.get_section(plugin->name);
    }

#endif
    return nullptr;
}

std::string WCM::find_icon(const std::string & name)
{
    // first try to find it in the user's local folders
    std::string icon_path;
    std::string xdg_data_dir;
    char *c_xdg_data_dir = std::getenv("XDG_DATA_HOME");
    char *c_user_home    = std::getenv("HOME");

    if (c_xdg_data_dir != NULL)
    {
        xdg_data_dir = c_xdg_data_dir;
    } else if (c_user_home != NULL)
    {
        xdg_data_dir = (std::string)c_user_home + "/.local/share/";
    }

    if (!xdg_data_dir.empty())
    {
        icon_path = xdg_data_dir + "/wayfire/icons/" + name;
        if (std::filesystem::exists(icon_path))
        {
            return icon_path;
        }
    }

    return ICONDIR "/" + name;
}

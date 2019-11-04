/*
 * Copyright © 2018 Scott Moreau
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
 */

#include <libevdev/libevdev.h>
#include <gdk/gdkwayland.h>
#include <iostream>
#include "wcm.h"

static int num_button_columns;

#define NUM_CATEGORIES 5

static void
reload_config(WCM *wcm)
{
        delete wcm->wf_config;

        load_config_file(wcm);
}

static void
get_button_position(Plugin *p, int *x, int *y)
{
        p->x = (*x)++;
        p->y = *y;
        if (*x > num_button_columns - 1) {
                *x = 0;
                (*y)++;
        }
}

static void
position_plugin_buttons(WCM *wcm)
{
        int i;
        Plugin *p;
        int x[NUM_CATEGORIES] = {};
        int y[NUM_CATEGORIES] = {};

        for (i = 0; i < int(wcm->plugins.size()); i++) {
                p = wcm->plugins[i];
                if (std::string(p->category) == "General")
                        get_button_position(p, &x[0], &y[0]);
                else if (std::string(p->category) == "Desktop")
                        get_button_position(p, &x[1], &y[1]);
                else if (std::string(p->category) == "Effects")
                        get_button_position(p, &x[2], &y[2]);
                else if (std::string(p->category) == "Window Management")
                        get_button_position(p, &x[3], &y[3]);
                else
                        get_button_position(p, &x[4], &y[4]);
        }
}

static gboolean
close_button_button_cb(GtkWidget *widget,
                GdkEventButton *event,
                gpointer user_data)
{
        int x = event->x;
        int y = event->y;
        int w = gtk_widget_get_allocated_width(widget);
        int h = gtk_widget_get_allocated_height(widget);

        if (x > 0 && y > 0 && x < w && y < h)
                exit(0);

        return true;
}

bool
close_button_key_cb(GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
        if (event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter)
                exit(0);

        return false;
}

static gboolean
back_button_cb(GtkWidget *widget,
               GdkEventButton *event,
               gpointer user_data)
{
        WCM *wcm = (WCM *) user_data;
        GtkWidget *window = wcm->window;
        GtkWidget *main_layout = wcm->main_layout;
        GtkWidget *plugin_layout = wcm->plugin_layout;

        gtk_widget_destroy(plugin_layout);

        gtk_container_add(GTK_CONTAINER(window), main_layout);
        g_object_unref(main_layout);

        gtk_widget_show_all(window);

        return true;
}

static bool begins_with(std::string word, std::string prefix)
{
    if (word.length() < prefix.length())
        return false;

    return word.substr(0, prefix.length()) == prefix;
}

static bool
all_chars_are(std::string str, char c)
{
        for (size_t i = 0; i < str.size(); i++)
                if (str[i] != c)
                        return false;
        return true;
}

static bool
not_in_list(std::vector<int> list, int item)
{
        for (size_t i = 0; i < list.size(); i++)
                if (list[i] == item)
                        return false;
        return true;
}

static void
add_option_widget(GtkWidget *widget, Option *o);

static void
add_command_item_button_cb(GtkWidget *widget,
                        GdkEventButton *event,
                        gpointer user_data)
{
        Option *o = (Option *) user_data;
        WCM *wcm = o->plugin->wcm;
        wayfire_config_section *section;
        wf_option option;
        GtkWidget *top_spacer;
        GList *children, *iter;
        auto exec_prefix = std::string("command_");
        std::vector<int> reorder_list;
        std::vector<std::string> command_names;
        std::vector<std::string> command_values;
        std::vector<std::string> alpha_names;
        std::vector<std::string> alpha_values;
        size_t count = 0;
        size_t i = 0, j = 0;

        section = wcm->wf_config->get_section(o->plugin->name);

        for (auto c : section->options) {
                if (begins_with(c->name, exec_prefix)) {
                        auto name = c->name.substr(exec_prefix.length());
                        auto slot = strtol(name.c_str(), NULL, 0);
                        if (!all_chars_are(name, '0') && !slot) {
                                alpha_names.push_back(c->name);
                                alpha_values.push_back(section->get_option(c->name, "")->as_string());
                        }
                        else if (not_in_list(reorder_list, slot)) {
                                reorder_list.push_back(slot);
                                command_names.push_back(c->name);
                                command_values.push_back(section->get_option(c->name, "")->as_string());
                                count++;
                        }
                } else {
                        alpha_names.push_back(c->name);
                        alpha_values.push_back(section->get_option(c->name, "")->as_string());
                }
                i++;
        }
        for (i = 0; i < count; i++) {
                if (not_in_list(reorder_list, i)) {
                        count = i;
                        break;
                }
        }
        auto name = std::string("command_") + std::to_string(count);
        command_names.push_back(name);
        command_values.push_back(std::string("<command>"));
        name = std::string("binding_") + std::to_string(count);
        alpha_names.push_back(name);
        alpha_values.push_back(std::string("<binding>"));
        reorder_list.push_back(count);
        auto sorted_list = reorder_list;
        std::sort(sorted_list.begin(), sorted_list.end());
        for (i = 0; i < sorted_list.size(); i++) {
                for (j = 0; j < reorder_list.size(); j++) {
                        if (sorted_list[i] == reorder_list[j]) {
                                std::swap(reorder_list[i], reorder_list[j]);
                                std::swap(command_names[i], command_names[j]);
                                std::swap(command_values[i], command_values[j]);
                                break;
                        }
                }
        }
        for (auto command : section->options)
        {
                option = section->get_option(command->name, "");
                option->set_value("");
        }

        wcm->wf_config->save_config(wcm->config_file);
        reload_config(wcm);
        section = wcm->wf_config->get_section(o->plugin->name);

        i = 0;
        for (auto command : command_names) {
                option = section->get_option(command, "");
                option->set_value(command_values[i]);
                i++;
        }
        i = 0;
        for (auto alpha_name : alpha_names) {
                option = section->get_option(alpha_name, "");
                option->set_value(alpha_values[i]);
                i++;
        }

        wcm->wf_config->save_config(wcm->config_file);
        reload_config(wcm);

        children = gtk_container_get_children(GTK_CONTAINER(o->widget));
        for(iter = children; iter != NULL; iter = g_list_next(iter))
                gtk_widget_destroy(GTK_WIDGET(iter->data));
        g_list_free(children);

        top_spacer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_widget_set_size_request(top_spacer, 1, 5);
        gtk_box_pack_start(GTK_BOX(o->widget), top_spacer, false, false, 0);
        add_option_widget(o->widget, o);
}

static void
remove_command_item_button_cb(GtkWidget *widget,
                           GdkEventButton *event,
                           gpointer user_data)
{
        Option *o = (Option *) user_data;
        WCM *wcm = o->plugin->wcm;
        wayfire_config_section *section;
        wf_option option;
        GtkWidget *top_spacer;
        GList *children, *iter;

        section = wcm->wf_config->get_section(o->plugin->name);
        for (size_t i = 0; i < o->parent->options.size(); i++) {
                Option *opt = o->parent->options[i];
                if (!strcmp(opt->name, o->name)) {
                        option = section->get_option(std::string(opt->name), "");
                        option->set_value("");
                        auto binding = std::string("binding_") + std::string(opt->name).substr(std::string("command_").length());
                        option = section->get_option(binding, "");
                        option->set_value("");
                        auto repeatable_binding = std::string("repeatable_binding_") + std::string(opt->name).substr(std::string("command_").length());
                        option = section->get_option(repeatable_binding, "");
                        option->set_value("");
                        auto always_binding = std::string("always_binding_") + std::string(opt->name).substr(std::string("command_").length());
                        option = section->get_option(always_binding, "");
                        option->set_value("");
                }
        }

        wcm->wf_config->save_config(wcm->config_file);
        reload_config(wcm);

        children = gtk_container_get_children(GTK_CONTAINER(o->widget));
        for(iter = children; iter != NULL; iter = g_list_next(iter))
                gtk_widget_destroy(GTK_WIDGET(iter->data));
        g_list_free(children);

        top_spacer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_widget_set_size_request(top_spacer, 1, 5);
        gtk_box_pack_start(GTK_BOX(o->widget), top_spacer, false, false, 0);
        add_option_widget(o->widget, o->parent);
}

static void
add_autostart_item_button_cb(GtkWidget *widget,
                        GdkEventButton *event,
                        gpointer user_data)
{
        Option *o = (Option *) user_data;
        WCM *wcm = o->plugin->wcm;
        wayfire_config_section *section;
        wf_option option;
        GtkWidget *top_spacer;
        GList *children, *iter;
        auto prefix = std::string("a");
        std::vector<int> slot_list;
        std::vector<std::string> names;
        std::vector<std::string> values;
        size_t i = 0;

        section = wcm->wf_config->get_section(o->plugin->name);

        for (auto c : section->options) {
                if (begins_with(c->name, prefix)) {
                        auto name = c->name.substr(prefix.length());
                        auto slot = strtol(name.c_str(), NULL, 0);
                        if (!all_chars_are(name, '0') && !slot) {
                                names.push_back(c->name);
                                values.push_back(section->get_option(c->name, "")->as_string());
                        }
                        else if (not_in_list(slot_list, slot)) {
                                slot_list.push_back(slot);
                                names.push_back(c->name);
                                values.push_back(section->get_option(c->name, "")->as_string());
                        }
                } else {
                        names.push_back(c->name);
                        values.push_back(section->get_option(c->name, "")->as_string());
                }
                i++;
        }
        for (i = 0; i < names.size(); i++) {
                if (not_in_list(slot_list, i)) {
                        break;
                }
        }

        auto name = std::string("a") + std::to_string(i);
        option = section->get_option(name, "");
        option->set_value("<command>");

        wcm->wf_config->save_config(wcm->config_file);

        children = gtk_container_get_children(GTK_CONTAINER(o->widget));
        for(iter = children; iter != NULL; iter = g_list_next(iter))
                gtk_widget_destroy(GTK_WIDGET(iter->data));
        g_list_free(children);

        top_spacer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_widget_set_size_request(top_spacer, 1, 5);
        gtk_box_pack_start(GTK_BOX(o->widget), top_spacer, false, false, 0);
        add_option_widget(o->widget, o);
}

static void
remove_autostart_item_button_cb(GtkWidget *widget,
                           GdkEventButton *event,
                           gpointer user_data)
{
        Option *o = (Option *) user_data;
        WCM *wcm = o->plugin->wcm;
        wayfire_config_section *section;
        wf_option option;
        GtkWidget *top_spacer;
        GList *children, *iter;

        section = wcm->wf_config->get_section(o->plugin->name);
        for (size_t i = 0; i < o->parent->options.size(); i++) {
                Option *opt = o->parent->options[i];
                if (!strcmp(opt->name, o->name)) {
                        option = section->get_option(std::string(opt->name), "");
                        option->set_value("");
                }
        }

        wcm->wf_config->save_config(wcm->config_file);
        reload_config(wcm);

        children = gtk_container_get_children(GTK_CONTAINER(o->widget));
        for(iter = children; iter != NULL; iter = g_list_next(iter))
                gtk_widget_destroy(GTK_WIDGET(iter->data));
        g_list_free(children);

        top_spacer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_widget_set_size_request(top_spacer, 1, 5);
        gtk_box_pack_start(GTK_BOX(o->widget), top_spacer, false, false, 0);
        add_option_widget(o->widget, o->parent);
}

static void
reset_button_cb(GtkWidget *widget,
                GdkEventButton *event,
                gpointer user_data)
{
        Option *o = (Option *) user_data;
        WCM *wcm = o->plugin->wcm;
        wayfire_config_section *section;
        wf_option option;

        switch (o->type) {
                case OPTION_TYPE_INT:
                        if (o->int_labels.size())
                                gtk_combo_box_set_active(GTK_COMBO_BOX(o->data_widget), o->default_value.i);
                        else
                                gtk_spin_button_set_value(GTK_SPIN_BUTTON(o->data_widget), o->default_value.i);
                        section = wcm->wf_config->get_section(o->plugin->name);
                        option = section->get_option(o->name, "");
                        option->set_value(o->default_value.i);
                        wcm->wf_config->save_config(wcm->config_file);
                        break;
                case OPTION_TYPE_BOOL:
                        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(o->data_widget), o->default_value.i);
                        section = wcm->wf_config->get_section(o->plugin->name);
                        option = section->get_option(o->name, "");
                        option->set_value(o->default_value.i);
                        wcm->wf_config->save_config(wcm->config_file);
                        break;
                case OPTION_TYPE_DOUBLE:
                        gtk_spin_button_set_value(GTK_SPIN_BUTTON(o->data_widget), o->default_value.d);
                        section = wcm->wf_config->get_section(o->plugin->name);
                        option = section->get_option(o->name, "");
                        option->set_value(o->default_value.d);
                        wcm->wf_config->save_config(wcm->config_file);
                        break;
                case OPTION_TYPE_BUTTON:
                case OPTION_TYPE_KEY:
                        gtk_button_set_label(GTK_BUTTON(o->data_widget), o->default_value.s);
                        section = wcm->wf_config->get_section(o->plugin->name);
                        option = section->get_option(o->name, "");
                        option->set_value(o->default_value.s);
                        wcm->wf_config->save_config(wcm->config_file);
                        break;
                case OPTION_TYPE_STRING:
                        if (o->str_labels.size()) {
                                LabeledString *ls;
                                int i;
                                for (i = 0; i < int(o->str_labels.size()); i++) {
                                        ls = o->str_labels[i];
                                        if (std::string(ls->value) == o->default_value.s) {
                                                gtk_combo_box_set_active(GTK_COMBO_BOX(o->data_widget), ls->id);
                                                break;
                                        }
                                }
                        } else {
                                gtk_entry_set_text(GTK_ENTRY(o->data_widget), o->default_value.s);
                        }
                        section = wcm->wf_config->get_section(o->plugin->name);
                        option = section->get_option(o->name, "");
                        option->set_value(o->default_value.s);
                        wcm->wf_config->save_config(wcm->config_file);
                        break;
                case OPTION_TYPE_COLOR:
                        GdkRGBA color;
                        if (sscanf(o->default_value.s, "%lf %lf %lf %lf", &color.red, &color.green, &color.blue, &color.alpha) != 4)
                                break;
                        gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(o->data_widget), &color);
                        section = wcm->wf_config->get_section(o->plugin->name);
                        option = section->get_option(o->name, "");
                        option->set_value(o->default_value.s);
                        wcm->wf_config->save_config(wcm->config_file);
                        break;
                default:
                        break;
        }
}

static void
set_string_combo_box_option_cb(GtkWidget *widget,
                               gpointer user_data)
{
        Option *o = (Option *) user_data;
        WCM *wcm = o->plugin->wcm;
        wayfire_config_section *section;
        wf_option option;
        LabeledString *ls = NULL;
        int i;

        section = wcm->wf_config->get_section(o->plugin->name);
        option = section->get_option(o->name, "");
        for (i = 0; i < int(o->str_labels.size()); i++) {
                ls = o->str_labels[i];
                if (ls->id == gtk_combo_box_get_active(GTK_COMBO_BOX(widget)))
                        break;
        }
        if (ls) {
                option->set_value(ls->value);
                wcm->wf_config->save_config(wcm->config_file);
        }
}

static gboolean
string_combo_box_focus_out_cb(GtkWidget *widget,
                              GdkEventButton *event,
                              gpointer user_data)
{
        set_string_combo_box_option_cb(widget, user_data);

        return GDK_EVENT_PROPAGATE;
}

static void
set_int_combo_box_option_cb(GtkWidget *widget,
                            gpointer user_data)
{
        Option *o = (Option *) user_data;
        WCM *wcm = o->plugin->wcm;
        wayfire_config_section *section;
        wf_option option;

        section = wcm->wf_config->get_section(o->plugin->name);
        option = section->get_option(o->name, "");
        option->set_value(gtk_combo_box_get_active(GTK_COMBO_BOX(widget)));
        wcm->wf_config->save_config(wcm->config_file);
}

static gboolean
int_combo_box_focus_out_cb(GtkWidget *widget,
                           GdkEventButton *event,
                           gpointer user_data)
{
        set_int_combo_box_option_cb(widget, user_data);

        return GDK_EVENT_PROPAGATE;
}

static void
spawn_color_chooser_cb(GtkWidget *widget,
                    GdkEventButton *event,
                    gpointer user_data)
{
        Option *o = (Option *) user_data;
        WCM *wcm = o->plugin->wcm;
        wayfire_config_section *section;
        wf_option option;
        GdkRGBA color;
        wf_color c;

        GtkWidget *chooser = gtk_color_chooser_dialog_new("Pick a Color", GTK_WINDOW(o->plugin->wcm->window));
        gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(widget), &color);
        gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(chooser), &color);

        if (gtk_dialog_run(GTK_DIALOG(chooser)) == GTK_RESPONSE_OK) {
                gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(chooser), &color);
                gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(widget), &color);
                section = wcm->wf_config->get_section(o->plugin->name);
                option = section->get_option(o->name, "");
                c.r = color.red;
                c.g = color.green;
                c.b = color.blue;
                c.a = color.alpha;
                option->set_value(c);
                wcm->wf_config->save_config(wcm->config_file);
        }

        gtk_widget_destroy(chooser);
}

static void
set_double_spin_button_option_cb(GtkWidget *widget,
                              gpointer user_data)
{
        Option *o = (Option *) user_data;
        WCM *wcm = o->plugin->wcm;
        wayfire_config_section *section;
        wf_option option;

        section = wcm->wf_config->get_section(o->plugin->name);
        option = section->get_option(o->name, "");
        option->set_value(gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget)));
        wcm->wf_config->save_config(wcm->config_file);
}

static gboolean
double_spin_button_focus_out_cb(GtkWidget *widget,
                                GdkEventButton *event,
                                gpointer user_data)
{
        set_double_spin_button_option_cb(widget, user_data);

        return GDK_EVENT_PROPAGATE;
}

static void
set_int_spin_button_option_cb(GtkWidget *widget,
                              gpointer user_data)
{
        Option *o = (Option *) user_data;
        WCM *wcm = o->plugin->wcm;
        wayfire_config_section *section;
        wf_option option;

        section = wcm->wf_config->get_section(o->plugin->name);
        option = section->get_option(o->name, "");
        option->set_value((int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget)));
        wcm->wf_config->save_config(wcm->config_file);
}

static gboolean
int_spin_button_focus_out_cb(GtkWidget *widget,
                             GdkEventButton *event,
                             gpointer user_data)
{
        set_int_spin_button_option_cb(widget, user_data);

        return GDK_EVENT_PROPAGATE;
}

static void
set_bool_check_button_option_cb(GtkWidget *widget,
                                gpointer user_data)
{
        Option *o = (Option *) user_data;
        WCM *wcm = o->plugin->wcm;
        wayfire_config_section *section;
        wf_option option;

        section = wcm->wf_config->get_section(o->plugin->name);
        option = section->get_option(o->name, "");
        option->set_value(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)) ? 1 : 0);
        wcm->wf_config->save_config(wcm->config_file);
}

static gboolean
bool_check_button_focus_out_cb(GtkWidget *widget,
                               GdkEventButton *event,
                               gpointer user_data)
{
        set_bool_check_button_option_cb(widget, user_data);

        return GDK_EVENT_PROPAGATE;
}

static void
set_string_option_cb(GtkWidget *widget,
                     gpointer user_data)
{
        Option *o = (Option *) user_data;
        WCM *wcm = o->plugin->wcm;
        wayfire_config_section *section;
        wf_option option;

        section = wcm->wf_config->get_section(o->plugin->name);
        option = section->get_option(o->name, "");
        option->set_value(gtk_entry_get_text(GTK_ENTRY(widget)));
        wcm->wf_config->save_config(wcm->config_file);
}

static gboolean
entry_focus_out_cb(GtkWidget *widget,
                   GdkEventButton *event,
                   gpointer user_data)
{
        set_string_option_cb(widget, user_data);

        return GDK_EVENT_PROPAGATE;
}

static std::string
get_command_from_index(std::string command,
                       wayfire_config_section *section,
                       int index)
{
    std::string option_name, b;
    wf_option option;

    b = "binding";
    option_name = command;

    switch (index) {
        case 0:
            option_name.replace(option_name.find(b),
                std::string(b).length(), "repeatable_binding");
            option = section->get_option(option_name, "");
            option->set_value("");
            option_name = command;
            option_name.replace(option_name.find(b),
                std::string(b).length(), "always_binding");
            option = section->get_option(option_name, "");
            option->set_value("");
            option_name = command;
            break;
        case 1:
            option = section->get_option(option_name, "");
            option->set_value("");
            option_name = command;
            option_name.replace(option_name.find(b),
                std::string(b).length(), "always_binding");
            option = section->get_option(option_name, "");
            option->set_value("");
            option_name = command;
            option_name.replace(option_name.find(b),
                std::string(b).length(), "repeatable_binding");
            break;
        case 2:
            option = section->get_option(option_name, "");
            option->set_value("");
            option_name = command;
            option_name.replace(option_name.find(b),
                std::string(b).length(), "repeatable_binding");
            option = section->get_option(option_name, "");
            option->set_value("");
            option_name = command;
            option_name.replace(option_name.find(b),
                std::string(b).length(), "always_binding");
            break;
        default:
            break;
    }

    return option_name;
}

static void
set_command_combo_box_option_cb(GtkWidget *widget,
                            gpointer user_data)
{
        Option *o = (Option *) user_data;
        WCM *wcm = o->plugin->wcm;
        wayfire_config_section *section;
        std::string option_name, value;
        wf_option option;
        int active;

        section = wcm->wf_config->get_section(o->plugin->name);
        active = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
        option_name = get_command_from_index(o->name, section, active);
        option = section->get_option(option_name, "");
        option->set_value(gtk_entry_get_text(GTK_ENTRY(o->binding_entry)));
        wcm->wf_config->save_config(wcm->config_file);
}

static gboolean
command_combo_box_focus_out_cb(GtkWidget *widget,
                           GdkEventButton *event,
                           gpointer user_data)
{
        set_command_combo_box_option_cb(widget, user_data);

        return GDK_EVENT_PROPAGATE;
}

static void
set_command_string_option_cb(GtkWidget *widget,
                     gpointer user_data)
{
        Option *o = (Option *) user_data;
        WCM *wcm = o->plugin->wcm;
        wayfire_config_section *section;
        std::string option_name;
        wf_option option;
        int active;

        section = wcm->wf_config->get_section(o->plugin->name);
        active = gtk_combo_box_get_active(GTK_COMBO_BOX(o->command_combo));
        option_name = get_command_from_index(o->name, section, active);
        option = section->get_option(option_name, "");
        option->set_value(gtk_entry_get_text(GTK_ENTRY(widget)));
        wcm->wf_config->save_config(wcm->config_file);
}

static gboolean
entry_command_focus_out_cb(GtkWidget *widget,
                   GdkEventButton *event,
                   gpointer user_data)
{
        set_command_string_option_cb(widget, user_data);

        return GDK_EVENT_PROPAGATE;
}

static void
set_binding_string_option_cb(GtkWidget *widget,
                     gpointer user_data)
{
        Option *o = (Option *) user_data;
        std::string label = o->name;

        label.erase(0, std::string("command_").length());
        label = std::string("Command " + label + ": " +
                gtk_entry_get_text(GTK_ENTRY(widget)));

        set_string_option_cb(widget, user_data);
        gtk_expander_set_label(GTK_EXPANDER(o->command_expander), label.c_str());
}

static gboolean
binding_entry_focus_out_cb(GtkWidget *widget,
                   GdkEventButton *event,
                   gpointer user_data)
{
        set_binding_string_option_cb(widget, user_data);

        return GDK_EVENT_PROPAGATE;
}

static GtkWidget *
create_plugins_layout(WCM *wcm);

static gboolean
main_panel_configure_cb(GtkWidget *widget,
                        GdkEvent *event,
                        gpointer user_data)
{
        WCM *wcm = (WCM *) user_data;
        int width = gtk_widget_get_allocated_width(widget) - gtk_widget_get_allocated_width(wcm->left_panel_layout);

        if ((width / 250) != num_button_columns) {
                num_button_columns = width / 250;
                position_plugin_buttons(wcm);
                gtk_widget_destroy(gtk_bin_get_child(GTK_BIN(wcm->scrolled_plugin_layout)));
                gtk_container_add(GTK_CONTAINER(wcm->scrolled_plugin_layout), create_plugins_layout(wcm));
                gtk_widget_show_all(wcm->window);
        }

        return false;
}

static void
setup_command_list(GtkWidget *widget, Option *o)
{
        WCM *wcm = o->plugin->wcm;
        wayfire_config_section *section;
        GtkWidget *option_layout, *combo_box, *label, *entry, *remove_button, *add_button, *add_button_layout;
        GtkWidget *list_add_image = gtk_image_new_from_icon_name("list-add", GTK_ICON_SIZE_BUTTON);
        GtkWidget *list_remove_image;
        wf_option option;

        section = wcm->wf_config->get_section(o->plugin->name);
        std::vector<std::string> command_names;
        const std::string exec_prefix = "command_";
        for (auto command : section->options)
        {
                if (begins_with(command->name, exec_prefix))
                        command_names.push_back(command->name.substr(exec_prefix.length()));
        }
        for (size_t i = 0; i < o->options.size(); i++) {
                Option *opt = o->options[i];
                free(opt->name);
                free(opt->default_value.s);
                delete opt;
        }
        o->options.clear();
        for (size_t i = 0; i < command_names.size(); i++) {
                auto command = exec_prefix + command_names[i];
                auto regular_binding_name = "binding_" + command_names[i];
                auto repeat_binding_name = "repeatable_binding_" + command_names[i];
                auto always_binding_name = "always_binding_" + command_names[i];

                auto executable = section->get_option(command, "")->as_string();
                auto regular_opt = section->get_option(regular_binding_name, "")->as_string();
                auto repeatable_opt = section->get_option(repeat_binding_name, "")->as_string();
                auto always_opt = section->get_option(always_binding_name, "")->as_string();
                GtkWidget *frame = gtk_frame_new(NULL);
                GtkWidget *expander = gtk_expander_new((std::string("Command ") + command_names[i] + ": " + executable).c_str());
                GtkWidget *expander_layout = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
                GtkWidget *options_layout = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
                option_layout = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
                combo_box = gtk_combo_box_text_new();
                gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_box), "Regular");
                gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_box), "Repeat");
                gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_box), "Always");
                if (!always_opt.empty())
                    gtk_combo_box_set_active(GTK_COMBO_BOX(combo_box), 2);
                else if (!repeatable_opt.empty())
                    gtk_combo_box_set_active(GTK_COMBO_BOX(combo_box), 1);
                else
                    gtk_combo_box_set_active(GTK_COMBO_BOX(combo_box), 0);
                label = gtk_label_new("Type");
                gtk_widget_set_margin_start(label, 10);
                gtk_widget_set_margin_end(label, 10);
                gtk_widget_set_size_request(label, 200, 1);
                gtk_label_set_xalign(GTK_LABEL(label), 0);
                gtk_box_pack_start(GTK_BOX(option_layout), label, false, false, 0);
                gtk_box_pack_start(GTK_BOX(option_layout), combo_box, true, true, 0);
                gtk_box_pack_start(GTK_BOX(options_layout), option_layout, true, true, 0);
                for (size_t j = 0; j < 2; j++) {
                        Option *dyn_opt = new Option();
                        option_layout = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
                        std::string label_text, opt_value;
                        if (j == 0) {
                                dyn_opt->name = strdup(regular_binding_name.c_str());
                                label_text = std::string("Binding");
                                opt_value = regular_opt;
                        } else if (j == 1) {
                                dyn_opt->name = strdup(command.c_str());
                                label_text = std::string("Command");
                                opt_value = executable;
                        }
                        if (std::string(o->default_value.s) == "string") {
                                dyn_opt->type = OPTION_TYPE_STRING;
                        } else {
                                continue;
                        }
                        o->widget = dyn_opt->widget = widget;
                        dyn_opt->parent = o;
                        dyn_opt->default_value.s = strdup(label_text.c_str());
                        dyn_opt->plugin = o->plugin;
                        label = gtk_label_new(label_text.c_str());
                        gtk_widget_set_margin_start(label, 10);
                        gtk_widget_set_margin_end(label, 10);
                        gtk_widget_set_size_request(label, 200, 1);
                        gtk_label_set_xalign(GTK_LABEL(label), 0);
                        entry = gtk_entry_new();
                        gtk_entry_set_text(GTK_ENTRY(entry), opt_value.c_str());
                        o->data_widget = entry;
                        if (j == 0) {
                            g_signal_connect(entry, "activate",
                                            G_CALLBACK(set_command_string_option_cb), dyn_opt);
                            g_signal_connect(entry, "focus-out-event",
                                            G_CALLBACK(entry_command_focus_out_cb), dyn_opt);
                            g_signal_connect(combo_box, "changed",
                                            G_CALLBACK(set_command_combo_box_option_cb), dyn_opt);
                            g_signal_connect(combo_box, "focus-out-event",
                                            G_CALLBACK(command_combo_box_focus_out_cb), dyn_opt);
                            dyn_opt->command_combo = combo_box;
                            dyn_opt->binding_entry = entry;
                        } else {
                            g_signal_connect(entry, "activate",
                                            G_CALLBACK(set_binding_string_option_cb), dyn_opt);
                            g_signal_connect(entry, "focus-out-event",
                                            G_CALLBACK(binding_entry_focus_out_cb), dyn_opt);
                            dyn_opt->command_expander = expander;
                        }
                        if (j == 1) {
                                remove_button = gtk_button_new();
                                gtk_widget_set_margin_start(remove_button, 10);
                                gtk_widget_set_margin_end(remove_button, 10);
                                gtk_widget_set_tooltip_text(remove_button, "Remove from list");
                                g_signal_connect(remove_button, "button-release-event",
                                                G_CALLBACK(remove_command_item_button_cb), dyn_opt);
                                list_remove_image = gtk_image_new_from_icon_name("list-remove", GTK_ICON_SIZE_BUTTON);
                                gtk_button_set_image(GTK_BUTTON(remove_button), list_remove_image);
                                gtk_box_pack_end(GTK_BOX(option_layout), remove_button, false, false, 0);
                        }
                        gtk_box_pack_start(GTK_BOX(option_layout), label, false, false, 0);
                        gtk_box_pack_end(GTK_BOX(option_layout), entry, true, true, 0);
                        gtk_box_pack_start(GTK_BOX(options_layout), option_layout, true, true, 0);
                        o->options.push_back(dyn_opt);
                }
                gtk_container_add(GTK_CONTAINER(expander), options_layout);
                gtk_container_add(GTK_CONTAINER(frame), expander);
                gtk_container_add(GTK_CONTAINER(expander_layout), frame);
                if (executable == std::string("<command>") || regular_opt == std::string("<binding>"))
                        gtk_expander_set_expanded(GTK_EXPANDER(expander), true);
                gtk_box_pack_start(GTK_BOX(widget), expander_layout, false, true, 0);
        }
        add_button_layout = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        add_button = gtk_button_new();
        gtk_widget_set_margin_start(add_button, 10);
        gtk_widget_set_margin_end(add_button, 10);
        gtk_widget_set_tooltip_text(add_button, "Add new command");
        g_signal_connect(add_button, "button-release-event",
                        G_CALLBACK(add_command_item_button_cb), o);
        gtk_button_set_image(GTK_BUTTON(add_button), list_add_image);
        gtk_box_pack_end(GTK_BOX(add_button_layout), add_button, false, false, 0);
        gtk_box_pack_end(GTK_BOX(widget), add_button_layout, false, true, 0);
        gtk_widget_show_all(widget);
}

static void
setup_autostart_list(GtkWidget *widget, Option *o)
{
        WCM *wcm = o->plugin->wcm;
        wayfire_config_section *section;
        GtkWidget *option_layout, *entry, *remove_button, *add_button, *add_button_layout;
        GtkWidget *list_add_image = gtk_image_new_from_icon_name("list-add", GTK_ICON_SIZE_BUTTON);
        GtkWidget *list_remove_image;
        section = wcm->wf_config->get_section(o->plugin->name);
        std::vector<std::string> autostart_names;
        for (auto e : section->options)
                autostart_names.push_back(e->name);
        for (size_t i = 0; i < o->options.size(); i++) {
                Option *opt = o->options[i];
                free(opt->name);
                free(opt->default_value.s);
                delete opt;
        }
        o->options.clear();
        for (size_t i = 0; i < autostart_names.size(); i++) {
                auto e = autostart_names[i];
                auto executable = section->get_option(e, "")->as_string();
                Option *dyn_opt = new Option();
                option_layout = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
                dyn_opt->name = strdup(e.c_str());
                if (std::string(o->default_value.s) == "string") {
                        dyn_opt->type = OPTION_TYPE_STRING;
                } else {
                        continue;
                }
                o->widget = dyn_opt->widget = widget;
                dyn_opt->parent = o;
                dyn_opt->default_value.s = strdup(executable.c_str());
                dyn_opt->plugin = o->plugin;
                entry = gtk_entry_new();
                gtk_entry_set_text(GTK_ENTRY(entry), executable.c_str());
                o->data_widget = entry;
                g_signal_connect(entry, "activate",
                                G_CALLBACK(set_string_option_cb), dyn_opt);
                g_signal_connect(entry, "focus-out-event",
                                G_CALLBACK(entry_focus_out_cb), dyn_opt);
                remove_button = gtk_button_new();
                gtk_widget_set_margin_start(remove_button, 10);
                gtk_widget_set_margin_end(remove_button, 10);
                gtk_widget_set_tooltip_text(remove_button, "Remove from list");
                g_signal_connect(remove_button, "button-release-event",
                                G_CALLBACK(remove_autostart_item_button_cb), dyn_opt);
                list_remove_image = gtk_image_new_from_icon_name("list-remove", GTK_ICON_SIZE_BUTTON);
                gtk_button_set_image(GTK_BUTTON(remove_button), list_remove_image);
                gtk_box_pack_end(GTK_BOX(option_layout), remove_button, false, false, 0);
                gtk_box_pack_end(GTK_BOX(option_layout), entry, true, true, 0);
                gtk_box_pack_start(GTK_BOX(widget), option_layout, false, true, 0);
                o->options.push_back(dyn_opt);
        }
        add_button_layout = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        add_button = gtk_button_new();
        gtk_widget_set_margin_start(add_button, 10);
        gtk_widget_set_margin_end(add_button, 10);
        gtk_widget_set_tooltip_text(add_button, "Add new command");
        g_signal_connect(add_button, "button-release-event",
                        G_CALLBACK(add_autostart_item_button_cb), o);
        gtk_button_set_image(GTK_BUTTON(add_button), list_add_image);
        gtk_box_pack_end(GTK_BOX(add_button_layout), add_button, false, false, 0);
        gtk_box_pack_end(GTK_BOX(widget), add_button_layout, false, true, 0);
        gtk_widget_show_all(widget);
}

static std::string
write_binding_option(Option *o, std::string name)
{
        WCM *wcm = o->plugin->wcm;
        wayfire_config_section *section;
        std::string text = "";
        bool first = true;
        wf_option option;

        section = wcm->wf_config->get_section(o->plugin->name);
        option = section->get_option(o->name, "");
        if (o->mod_mask & MOD_TYPE_SHIFT) {
                text += "<shift>";
                first = false;
        }
        if (o->mod_mask & MOD_TYPE_CONTROL) {
                if (!first)
                        text += " ";
                text += "<ctrl>";
                first = false;
        }
        if (o->mod_mask & MOD_TYPE_ALT) {
                if (!first)
                        text += " ";
                text += "<alt>";
                first = false;
        }
        if (o->mod_mask & MOD_TYPE_SUPER) {
                if (!first)
                        text += " ";
                text += "<super>";
                first = false;
        }
        if (!first)
                text += " ";
        text += name;

        option->set_value(text.c_str());
        wcm->wf_config->save_config(wcm->config_file);

        return text;
}

static void
grab_binding_button_cb(GtkWidget *widget,
                       GdkEventButton *event,
                       gpointer user_data)
{
        Option *o = (Option *) user_data;
        std::string opt_text = "";

        if (event->type != GDK_BUTTON_PRESS)
                return;

        switch (event->button) {
                case 1:
                        opt_text = write_binding_option(o, "BTN_LEFT");
                        break;
                case 2:
                        opt_text = write_binding_option(o, "BTN_MIDDLE");
                        break;
                case 3:
                        opt_text = write_binding_option(o, "BTN_RIGHT");
                        break;
                case 4:
                        opt_text = write_binding_option(o, "BTN_SIDE");
                        break;
                case 5:
                        opt_text = write_binding_option(o, "BTN_EXTRA");
                        break;
                default:
                        break;
        }

        gtk_button_set_label(GTK_BUTTON(o->data_widget), opt_text.c_str());
        gtk_window_close(GTK_WINDOW(widget));
        if (o->plugin->wcm->screen_lock) {
                zwlr_input_inhibitor_v1_destroy(o->plugin->wcm->screen_lock);
                o->plugin->wcm->screen_lock = NULL;
                wl_display_flush(gdk_wayland_display_get_wl_display(gdk_display_get_default()));
        }
}

#define HW_OFFSET 8

bool
grab_binding_key_cb(GtkWidget *widget,
                    GdkEventKey *event,
                    gpointer user_data)
{
        Option *o = (Option *) user_data;

        if (event->type == GDK_KEY_PRESS) {
                if (event->keyval == GDK_KEY_Shift_L || event->keyval == GDK_KEY_Shift_R) {
                        o->mod_mask = (mod_type) (o->mod_mask | MOD_TYPE_SHIFT);
                } else if (event->keyval == GDK_KEY_Control_L || event->keyval == GDK_KEY_Control_R) {
                        o->mod_mask = (mod_type) (o->mod_mask | MOD_TYPE_CONTROL);
                } else if (event->keyval == GDK_KEY_Alt_L || event->keyval == GDK_KEY_Alt_R ||
                        event->keyval == GDK_KEY_Meta_L || event->keyval == GDK_KEY_Meta_R) {
                        o->mod_mask = (mod_type) (o->mod_mask | MOD_TYPE_ALT);
                } else if (event->keyval == GDK_KEY_Super_L || event->keyval == GDK_KEY_Super_R) {
                        o->mod_mask = (mod_type) (o->mod_mask | MOD_TYPE_SUPER);
                } else {
                        gtk_button_set_label(GTK_BUTTON(o->data_widget),
                                write_binding_option(o,
                                libevdev_event_code_get_name(EV_KEY, event->hardware_keycode - HW_OFFSET)).c_str());
                        gtk_window_close(GTK_WINDOW(widget));
                        if (o->plugin->wcm->screen_lock) {
                                zwlr_input_inhibitor_v1_destroy(o->plugin->wcm->screen_lock);
                                o->plugin->wcm->screen_lock = NULL;
                                wl_display_flush(gdk_wayland_display_get_wl_display(gdk_display_get_default()));
                        }
                }
        }

        if (event->type == GDK_KEY_RELEASE) {
                if (event->keyval == GDK_KEY_Shift_L || event->keyval == GDK_KEY_Shift_R) {
                        o->mod_mask = (mod_type) (o->mod_mask & ~MOD_TYPE_SHIFT);
                } else if (event->keyval == GDK_KEY_Control_L || event->keyval == GDK_KEY_Control_R) {
                        o->mod_mask = (mod_type) (o->mod_mask & ~MOD_TYPE_CONTROL);
                } else if (event->keyval == GDK_KEY_Alt_L || event->keyval == GDK_KEY_Alt_R ||
                        event->keyval == GDK_KEY_Meta_L || event->keyval == GDK_KEY_Meta_R) {
                        o->mod_mask = (mod_type) (o->mod_mask & ~MOD_TYPE_ALT);
                } else if (event->keyval == GDK_KEY_Super_L || event->keyval == GDK_KEY_Super_R) {
                        o->mod_mask = (mod_type) (o->mod_mask & ~MOD_TYPE_SUPER);
                }
        }

        std::string modifiers = "";

        if (o->mod_mask & MOD_TYPE_SHIFT)
                modifiers += "<Shift>";

        if (o->mod_mask & MOD_TYPE_CONTROL)
                modifiers += "<Control>";

        if (o->mod_mask & MOD_TYPE_ALT)
                modifiers += "<Alt>";

        if (o->mod_mask & MOD_TYPE_SUPER)
                modifiers += "<Super>";

        gtk_label_set_text(GTK_LABEL(o->label_widget), modifiers.c_str());

        return false;
}

static void registry_add_object(void *data, struct wl_registry *registry,
    uint32_t name, const char *interface, uint32_t version)
{
        WCM *wcm = (WCM *) data;

        if (strcmp(interface, zwlr_input_inhibit_manager_v1_interface.name) == 0)
        {
                wcm->inhibitor_manager = (zwlr_input_inhibit_manager_v1*) wl_registry_bind(
                        registry, name, &zwlr_input_inhibit_manager_v1_interface, 1u);
        }
}

static void registry_remove_object(void *data, struct wl_registry *registry, uint32_t name)
{
}

static struct wl_registry_listener registry_listener =
{
        &registry_add_object,
        &registry_remove_object
};

static bool
lock_input(WCM *wcm)
{

        struct wl_display *display = gdk_wayland_display_get_wl_display(gdk_display_get_default());
        if (!display)
        {
            std::cerr << "display == NULL" << std::endl;
            return false;
        }
        struct wl_registry *registry = wl_display_get_registry(display);
        if (!registry)
        {
            std::cerr << "registry == NULL" << std::endl;
            return false;
        }

        wl_registry_add_listener(registry, &registry_listener, wcm);
        wl_display_dispatch(display);
        wl_display_roundtrip(display);

        if (!wcm->inhibitor_manager)
        {
            std::cerr << "Compositor does not support " <<
                " wlr_input_inhibit_manager_v1!" << std::endl;
            return false;
        }

        /* Lock input */
        if (wcm->inhibitor_manager)
        {
            wcm->screen_lock = zwlr_input_inhibit_manager_v1_get_inhibitor(wcm->inhibitor_manager);
        }

        return true;
}

static gboolean
window_deleted_cb(GtkWidget *widget,
                  GdkEvent *event,
                  gpointer user_data)
{
        Option *o = (Option *) user_data;

        if (!o->plugin->wcm->screen_lock)
                 return false;

        zwlr_input_inhibitor_v1_destroy(o->plugin->wcm->screen_lock);
        o->plugin->wcm->screen_lock = NULL;
        wl_display_flush(gdk_wayland_display_get_wl_display(gdk_display_get_default()));

        return false;
}

static void
key_grab_button_cb(GtkWidget *widget,
                   GdkEventButton *event,
                   gpointer user_data)
{
        Option *o = (Option *) user_data;

        if (!lock_input(o->plugin->wcm))
                return;

        o->mod_mask = (mod_type) 0;
        GtkWidget *grab_binding_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_title(GTK_WINDOW(grab_binding_window), "Waiting for Binding");
        g_signal_connect(grab_binding_window, "button-press-event",
                                         G_CALLBACK(grab_binding_button_cb), o);
        g_signal_connect(grab_binding_window, "key-press-event",
                                         G_CALLBACK(grab_binding_key_cb), o);
        g_signal_connect(grab_binding_window, "key-release-event",
                                         G_CALLBACK(grab_binding_key_cb), o);
        g_signal_connect(grab_binding_window, "delete-event",
                                         G_CALLBACK(window_deleted_cb), o);

        GtkWidget *layout = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        GtkWidget *label = gtk_label_new(NULL);
        gtk_box_pack_start(GTK_BOX(layout), label, true, true, 0);
        gtk_container_add(GTK_CONTAINER(grab_binding_window), layout);
        o->label_widget = label;

        gtk_widget_show_all(grab_binding_window);
}

static void
binding_cancel_cb(GtkWidget *widget,
                  GdkEventButton *event,
                  gpointer user_data)
{
        gtk_window_close(GTK_WINDOW(user_data));
}

static void
write_option(GtkWidget *widget,
             gpointer user_data)
{
        Option *o = (Option *) user_data;
        WCM *wcm = o->plugin->wcm;
        wayfire_config_section *section;
        wf_option option;

        section = wcm->wf_config->get_section(o->plugin->name);
        option = section->get_option(o->name, "");

        option->set_value(gtk_entry_get_text(GTK_ENTRY(o->label_widget)));
        wcm->wf_config->save_config(wcm->config_file);

        gtk_button_set_label(GTK_BUTTON(o->data_widget), gtk_entry_get_text(GTK_ENTRY(o->label_widget)));

        gtk_window_close(GTK_WINDOW(o->edit_window));
}

static void
binding_ok_cb(GtkWidget *widget,
              GdkEventButton *event,
              gpointer user_data)
{
        write_option(widget, user_data);
}

static void
binding_entry_cb(GtkWidget *widget,
                 gpointer user_data)
{
        write_option(widget, user_data);
}

static void
binding_edit_button_cb(GtkWidget *widget,
                       GdkEventButton *event,
                       gpointer user_data)
{
        Option *o = (Option *) user_data;
        WCM *wcm = o->plugin->wcm;
        wayfire_config_section *section;
        wf_option option;

        section = wcm->wf_config->get_section(o->plugin->name);
        option = section->get_option(o->name, o->default_value.s);

        GtkWidget *edit_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_title(GTK_WINDOW(edit_window), "Edit Binding");

        GtkWidget *layout = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        GtkWidget *button_layout = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        GtkWidget *button_cancel = gtk_button_new_with_label("Cancel");
        GtkWidget *button_ok = gtk_button_new_with_label("Ok");
        GtkWidget *entry = gtk_entry_new();
        gtk_widget_set_margin_top(layout, 10);
        gtk_widget_set_margin_bottom(layout, 10);
        gtk_widget_set_margin_start(layout, 10);
        gtk_widget_set_margin_end(layout, 10);
        g_signal_connect(button_cancel, "button-release-event",
                G_CALLBACK(binding_cancel_cb), edit_window);
        g_signal_connect(button_ok, "button-release-event",
                G_CALLBACK(binding_ok_cb), o);
        g_signal_connect(entry, "activate",
                G_CALLBACK(binding_entry_cb), o);
        gtk_entry_set_text(GTK_ENTRY(entry), option->as_string().c_str());
        gtk_box_pack_start(GTK_BOX(layout), entry, false, false, 0);
        gtk_box_pack_end(GTK_BOX(button_layout), button_ok, false, false, 0);
        gtk_box_pack_end(GTK_BOX(button_layout), button_cancel, false, false, 0);
        gtk_box_pack_end(GTK_BOX(layout), button_layout, false, true, 0);
        gtk_container_add(GTK_CONTAINER(edit_window), layout);
        o->label_widget = entry;
        o->edit_window = edit_window;

        gtk_widget_show_all(edit_window);
}

static void
add_option_widget(GtkWidget *widget, Option *o)
{
        WCM *wcm = o->plugin->wcm;
        GtkWidget *option_layout, *label, *reset_button, *reset_image;
        wayfire_config_section *section;
        wf_option option;

        reset_image = gtk_image_new_from_icon_name("edit-clear", GTK_ICON_SIZE_BUTTON);

        switch (o->type) {
                case OPTION_TYPE_INT:
                case OPTION_TYPE_BOOL:
                case OPTION_TYPE_DOUBLE:
                case OPTION_TYPE_BUTTON:
                case OPTION_TYPE_KEY:
                case OPTION_TYPE_STRING:
                case OPTION_TYPE_COLOR:
                        section = wcm->wf_config->get_section(o->plugin->name);
                        option_layout = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
                        label = gtk_label_new(o->disp_name);
                        gtk_widget_set_margin_start(label, 10);
                        gtk_widget_set_margin_end(label, 10);
                        gtk_widget_set_tooltip_text(label, o->name);
                        gtk_widget_set_size_request(label, 200, 1);
                        gtk_label_set_xalign(GTK_LABEL(label), 0);
                        reset_button = gtk_button_new();
                        gtk_widget_set_margin_start(reset_button, 10);
                        gtk_widget_set_margin_end(reset_button, 10);
                        gtk_widget_set_tooltip_text(reset_button, "Reset to default");
                        g_signal_connect(reset_button, "button-release-event",
                                         G_CALLBACK(reset_button_cb), o);
                        gtk_button_set_image(GTK_BUTTON(reset_button), reset_image);
                        gtk_box_pack_start(GTK_BOX(option_layout), label, false, false, 0);
                        gtk_box_pack_end(GTK_BOX(option_layout), reset_button, false, false, 0);
                        break;
                case OPTION_TYPE_DYNAMIC_LIST:
                                if (std::string(o->name) == "command")
                                        setup_command_list(widget, o);
                                else if (std::string(o->name) == "autostart")
                                        setup_autostart_list(widget, o);
                        break;
                default:
                        break;
        }

        switch (o->type) {
                case OPTION_TYPE_INT: {
                        int i;
                        LabeledInt *li;
                        option = section->get_option(o->name, std::to_string(o->default_value.i));
                        GtkWidget *combo_box;
                        GtkWidget *spin_button;
                        if (o->int_labels.size()) {
                                combo_box = gtk_combo_box_text_new();
                                for (i = 0; i < int(o->int_labels.size()); i++) {
                                        li = o->int_labels[i];
                                        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_box), li->name);
                                }
                                gtk_combo_box_set_active(GTK_COMBO_BOX(combo_box), option->as_int());
                                o->data_widget = combo_box;
                                g_signal_connect(combo_box, "changed",
                                                 G_CALLBACK(set_int_combo_box_option_cb), o);
                                g_signal_connect(combo_box, "focus-out-event",
                                                 G_CALLBACK(int_combo_box_focus_out_cb), o);
                                gtk_box_pack_end(GTK_BOX(option_layout), combo_box, true, true, 0);
                        } else {
                                spin_button = gtk_spin_button_new(gtk_adjustment_new(option->as_int(), o->data.min, o->data.max, 1, 10, 0), 1, 0);
                                o->data_widget = spin_button;
                                g_signal_connect(spin_button, "activate",
                                                 G_CALLBACK(set_int_spin_button_option_cb), o);
                                g_signal_connect(spin_button, "changed",
                                                 G_CALLBACK(set_int_spin_button_option_cb), o);
                                g_signal_connect(spin_button, "focus-out-event",
                                                 G_CALLBACK(int_spin_button_focus_out_cb), o);
                                gtk_box_pack_end(GTK_BOX(option_layout), spin_button, false, true, 0);
                        }
                        gtk_box_pack_start(GTK_BOX(widget), option_layout, false, true, 0);
                }
                        break;
                case OPTION_TYPE_BOOL: {
                        option = section->get_option(o->name, std::to_string(o->default_value.i));
                        GtkWidget *check_button = gtk_check_button_new();
                        section = wcm->wf_config->get_section(o->plugin->name);
                        option = section->get_option(o->name, std::to_string(o->default_value.i));
                        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_button), option->as_int() ? 1 : 0);
                        o->data_widget = check_button;
                        g_signal_connect(check_button, "toggled",
                                         G_CALLBACK(set_bool_check_button_option_cb), o);
                        g_signal_connect(check_button, "focus-out-event",
                                         G_CALLBACK(bool_check_button_focus_out_cb), o);
                        gtk_box_pack_end(GTK_BOX(option_layout), check_button, false, true, 0);
                        gtk_box_pack_start(GTK_BOX(widget), option_layout, false, true, 0);
                }
                        break;
                case OPTION_TYPE_DOUBLE: {
                        option = section->get_option(o->name, std::to_string(o->default_value.d));
                        GtkWidget *spin_button = gtk_spin_button_new(gtk_adjustment_new(option->as_double(), o->data.min, o->data.max, o->data.precision, 0, 0), o->data.precision, 3);
                        o->data_widget = spin_button;
                        g_signal_connect(spin_button, "activate",
                                         G_CALLBACK(set_double_spin_button_option_cb), o);
                        g_signal_connect(spin_button, "changed",
                                         G_CALLBACK(set_double_spin_button_option_cb), o);
                        g_signal_connect(spin_button, "focus-out-event",
                                         G_CALLBACK(double_spin_button_focus_out_cb), o);
                        gtk_box_pack_end(GTK_BOX(option_layout), spin_button, false, true, 0);
                        gtk_box_pack_start(GTK_BOX(widget), option_layout, false, true, 0);
                }
                        break;
                case OPTION_TYPE_BUTTON:
                case OPTION_TYPE_KEY: {
                        option = section->get_option(o->name, o->default_value.s);
                        GtkWidget *edit_button = gtk_button_new_from_icon_name("gtk-edit", GTK_ICON_SIZE_BUTTON);
                        g_signal_connect(edit_button, "button-release-event",
                                        G_CALLBACK(binding_edit_button_cb), o);
                        GtkWidget *key_grab_button = gtk_button_new_with_label(option->as_string().c_str());
                        g_signal_connect(key_grab_button, "button-release-event",
                                        G_CALLBACK(key_grab_button_cb), o);
                        o->data_widget = key_grab_button;
                        gtk_box_pack_end(GTK_BOX(option_layout), edit_button, false, true, 0);
                        gtk_widget_set_margin_start(edit_button, 10);
                        gtk_box_pack_end(GTK_BOX(option_layout), key_grab_button, false, true, 0);
                        gtk_box_pack_start(GTK_BOX(widget), option_layout, false, true, 0);
                }
                        break;
                case OPTION_TYPE_STRING: {
                        int i;
                        LabeledString *ls;
                        GtkWidget *combo_box;
                        GtkWidget *entry;
                        option = section->get_option(o->name, o->default_value.s);
                        if (o->str_labels.size()) {
                                combo_box = gtk_combo_box_text_new();
                                for (i = 0; i < int(o->str_labels.size()); i++) {
                                        ls = o->str_labels[i];
                                        ls->id = i;
                                        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_box), ls->name);
                                        if (ls->value == option->as_string())
                                                gtk_combo_box_set_active(GTK_COMBO_BOX(combo_box), i);
                                }
                                o->data_widget = combo_box;
                                g_signal_connect(combo_box, "changed",
                                                 G_CALLBACK(set_string_combo_box_option_cb), o);
                                g_signal_connect(combo_box, "focus-out-event",
                                                 G_CALLBACK(string_combo_box_focus_out_cb), o);
                                gtk_box_pack_end(GTK_BOX(option_layout), combo_box, true, true, 0);
                        } else {
                                entry = gtk_entry_new();
                                gtk_entry_set_text(GTK_ENTRY(entry), option->as_string().c_str());
                                o->data_widget = entry;
                                g_signal_connect(entry, "activate",
                                                 G_CALLBACK(set_string_option_cb), o);
                                g_signal_connect(entry, "focus-out-event",
                                                 G_CALLBACK(entry_focus_out_cb), o);
                                gtk_box_pack_end(GTK_BOX(option_layout), entry, true, true, 0);
                        }
                        gtk_box_pack_start(GTK_BOX(widget), option_layout, false, true, 0);
                }
                        break;
                case OPTION_TYPE_COLOR: {
                        wf_color c;
                        GdkRGBA color;
                        option = section->get_option(o->name, o->default_value.s);
                        c = option->as_color();
                        color.red = c.r;
                        color.green = c.g;
                        color.blue = c.b;
                        color.alpha = c.a;
                        GtkWidget *color_button = gtk_color_button_new_with_rgba(&color);
                        o->data_widget = color_button;
                        g_signal_connect(color_button, "button-release-event",
                                         G_CALLBACK(spawn_color_chooser_cb), o);
                        gtk_box_pack_end(GTK_BOX(option_layout), color_button, false, false, 0);
                        gtk_box_pack_start(GTK_BOX(widget), option_layout, false, true, 0);
                }
                        break;
                default:
                        break;
        }
}

static void
toggle_plugin_enabled_cb(GtkWidget *widget,
                         gpointer user_data)
{
        Plugin *p = (Plugin *) user_data;
        WCM *wcm = p->wcm;
        wayfire_config_section *section;
        wf_option option;
        std::string plugins;
        size_t pos;

        section = wcm->wf_config->get_section("core");
        option = section->get_option("plugins", "default");

        p->enabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
        plugins = option->as_string();

        if (p->enabled) {
                pos = plugins.find(std::string(p->name));
                if (pos == std::string::npos) {
                        option->set_value(option->as_string() + " " + std::string(p->name));
                        wcm->wf_config->save_config(wcm->config_file);
                }
                if (widget == p->t1)
                        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(p->t2), true);
                else if (widget == p->t2)
                        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(p->t1), true);
        } else {
                int i;
                /* Remove plugin name string */
                pos = plugins.find(std::string(p->name));
                if (pos != std::string::npos)
                        plugins.erase(pos, std::string(p->name).length());
                /* Remove trailing spaces */
                pos = plugins.find_last_not_of(" ");
                if (pos != std::string::npos)
                        plugins.substr(0, pos + 1);
                /* Remove duplicate spaces */
                for (i = plugins.size() - 1; i >= 0; i--)
                        if(plugins[i] == ' ' && plugins[i] == plugins[i - 1])
                                plugins.erase(plugins.begin() + i);
                option->set_value(plugins);
                wcm->wf_config->save_config(wcm->config_file);
                if (widget == p->t1)
                        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(p->t2), false);
                else if (widget == p->t2)
                        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(p->t1), false);
        }
}

static gboolean
plugin_button_cb(GtkWidget *widget,
                 GdkEventButton *event,
                 gpointer user_data)
{
        Plugin *p = (Plugin *) user_data;
        Option *g, *s, *o;
        WCM *wcm = p->wcm;
        GtkWidget *window = wcm->window;
        GtkWidget *main_layout = wcm->main_layout;
        GtkWidget *enable_label, *enabled_cb;
        int i, j, k;

        g_object_ref(main_layout);
        gtk_container_remove(GTK_CONTAINER(window), main_layout);

        GtkWidget *plugin_layout = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        GtkWidget *left_panel_layout = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        GtkWidget *plugin_buttons_layout = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        GtkWidget *label = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(label), ("<span size=\"12000\"><b>" + std::string(p->disp_name) + "</b></span>").c_str());
        g_object_set(label, "margin", 50, NULL);
        gtk_label_set_line_wrap(GTK_LABEL(label), true);
        gtk_label_set_max_width_chars(GTK_LABEL(label), 15);
        GtkWidget *enable_layout = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        if (std::string(p->name) != "core" && std::string(p->name) != "input") {
                enable_label = gtk_label_new(NULL);
                gtk_label_set_markup(GTK_LABEL(enable_label), "<span size=\"10000\"><b>Use This Plugin</b></span>");
                enabled_cb = gtk_check_button_new();
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enabled_cb), p->enabled ? true : false);
                gtk_widget_set_margin_start(enabled_cb, 50);
                gtk_widget_set_margin_end(enabled_cb, 15);
                g_signal_connect(enabled_cb, "toggled",
                                G_CALLBACK(toggle_plugin_enabled_cb), p);
                p->t2 = enabled_cb;
        }
        GtkWidget *back_button = gtk_button_new();
        GtkWidget *back_layout = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        GtkWidget *back_image = gtk_image_new_from_icon_name("back", GTK_ICON_SIZE_BUTTON);
        GtkWidget *back_label = gtk_label_new("Back");
        gtk_widget_set_size_request(back_layout, 70, -1);
        gtk_widget_set_margin_start(back_layout, 70);
        gtk_widget_set_margin_end(back_layout, 70);
        gtk_box_pack_start(GTK_BOX(back_layout), back_image, true, false, 0);
        gtk_box_pack_end(GTK_BOX(back_layout), back_label, true, false, 0);
        gtk_container_add(GTK_CONTAINER(back_button), back_layout);
        g_object_set(back_button, "margin", 10, NULL);
        g_signal_connect(back_button, "button-release-event",
                         G_CALLBACK(back_button_cb), wcm);
        GtkWidget *notebook = gtk_notebook_new();
        gtk_notebook_set_tab_pos(GTK_NOTEBOOK(notebook), GTK_POS_TOP);
        GtkWidget *scrolled_window;
        GtkWidget *options_layout;
        GtkWidget *top_spacer;
        for (i = 0; i < int(p->options.size()); i++) {
                g = p->options[i];
                if (g->type != OPTION_TYPE_GROUP)
                        continue;
                options_layout = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
                top_spacer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
                gtk_widget_set_size_request(top_spacer, 1, 5);
                gtk_box_pack_start(GTK_BOX(options_layout), top_spacer, false, false, 0);
                for (j = 0; j < int(g->options.size()); j++) {
                        o = g->options[j];
                        if (o->type == OPTION_TYPE_SUBGROUP && int(o->options.size())) {
                                GtkWidget *frame = gtk_frame_new(NULL);
                                GtkWidget *expander = gtk_expander_new(o->name);
                                GtkWidget *expander_layout = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
                                for (k = 0; k < int(o->options.size()); k++) {
                                        s = o->options[k];
                                        add_option_widget(expander_layout, s);
                                }
                                gtk_container_add(GTK_CONTAINER(expander), expander_layout);
                                gtk_container_add(GTK_CONTAINER(frame), expander);
                                gtk_container_add(GTK_CONTAINER(options_layout), frame);
                        } else {
                                add_option_widget(options_layout, o);
                        }
                }
                scrolled_window = gtk_scrolled_window_new(NULL, NULL);
                gtk_widget_set_vexpand(scrolled_window, true);
                gtk_container_add(GTK_CONTAINER(scrolled_window), options_layout);
                gtk_notebook_append_page(GTK_NOTEBOOK(notebook), scrolled_window, gtk_label_new(g->name));
        }
        gtk_box_pack_start(GTK_BOX(plugin_buttons_layout), notebook, false, true, 10);
        gtk_box_pack_start(GTK_BOX(left_panel_layout), label, false, false, 0);
        if (std::string(p->name) != "core" && std::string(p->name) != "input") {
                gtk_box_pack_start(GTK_BOX(enable_layout), enabled_cb, false, false, 0);
                gtk_box_pack_start(GTK_BOX(enable_layout), enable_label, false, false, 0);
        }
        gtk_box_pack_start(GTK_BOX(left_panel_layout), enable_layout, false, false, 0);
        gtk_box_pack_end(GTK_BOX(left_panel_layout), back_button, false, false, 0);
        gtk_box_pack_start(GTK_BOX(plugin_layout), left_panel_layout, false, true, 0);
        gtk_box_pack_end(GTK_BOX(plugin_layout), plugin_buttons_layout, true, true, 0);
        gtk_container_add(GTK_CONTAINER(window), plugin_layout);

        wcm->plugin_layout = plugin_layout;

        gtk_widget_show_all(window);

        return true;
}

static const char *
get_icon_name_from_category(std::string category)
{
        if (category == "General")
                return "preferences-system";
        else if (category == "Desktop")
                return "preferences-desktop";
        else if (category == "Effects")
                return "applications-graphics";
        else if (category == "Window Management")
                return "applications-office";
        else
                return "applications-other";
}

static void
add_plugin_to_category(Plugin *p, GtkWidget **category, GtkWidget **layout)
{
        if (!*category) {
                *layout = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
                GtkWidget *header_layout = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
                *category = gtk_grid_new();
                gtk_grid_set_row_homogeneous(GTK_GRID(*category), true);
                GtkWidget *icon = gtk_image_new_from_icon_name(get_icon_name_from_category(std::string(p->category)), GTK_ICON_SIZE_DND);
                GtkWidget *label = gtk_label_new(NULL);
                gtk_label_set_markup(GTK_LABEL(label), ("<span size=\"14000\" color=\"#AAA\"><b>" + std::string(p->category) + "</b></span>").c_str());
                g_object_set(icon, "margin", 10, NULL);
                gtk_box_pack_start(GTK_BOX(header_layout), icon, false, false, 0);
                gtk_box_pack_start(GTK_BOX(header_layout), label, false, false, 0);
                gtk_container_add(GTK_CONTAINER(*layout), header_layout);
                gtk_container_add(GTK_CONTAINER(*layout), *category);
        }
        GtkWidget *button_layout = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        GtkWidget *plugin_layout = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        GtkWidget *check_button;
        if (std::string(p->name) != "core" && std::string(p->name) != "input") {
                check_button = gtk_check_button_new();
                g_object_set(check_button, "margin", 5, NULL);
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_button), p->enabled ? true : false);
                g_signal_connect(check_button, "toggled",
                                 G_CALLBACK(toggle_plugin_enabled_cb), p);
                p->t1 = check_button;
        }
        GtkWidget *plugin_button = gtk_button_new();
        gtk_button_set_relief(GTK_BUTTON(plugin_button), GTK_RELIEF_NONE);
        gtk_widget_set_size_request(plugin_button, 200, 1);
        GtkWidget *button_icon = gtk_image_new_from_file((ICONDIR "/plugin-" + std::string(p->name) + ".svg").c_str());
        GtkWidget *button_label = gtk_label_new(p->disp_name);
        gtk_box_pack_start(GTK_BOX(button_layout), button_icon, false, false, 0);
        gtk_box_pack_start(GTK_BOX(button_layout), button_label, false, false, 0);
        gtk_container_add(GTK_CONTAINER(plugin_button), button_layout);
        if (std::string(p->name) != "core" && std::string(p->name) != "input")
                gtk_box_pack_start(GTK_BOX(plugin_layout), check_button, false, false, 0);
        else
                gtk_widget_set_margin_start(plugin_button, 25);
        gtk_box_pack_start(GTK_BOX(plugin_layout), plugin_button, false, false, 0);
        g_object_set(plugin_layout, "margin", 5, NULL);
        gtk_grid_attach(GTK_GRID(*category), plugin_layout, p->x, p->y, 1, 1);
        g_signal_connect(plugin_button, "button-release-event",
                         G_CALLBACK(plugin_button_cb), p);
}

static GtkWidget *
create_plugins_layout(WCM *wcm)
{
        Plugin *p;
        int i;

        GtkWidget *plugin_buttons_layout = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

        GtkWidget *categories[NUM_CATEGORIES] = {};
        GtkWidget *layout[NUM_CATEGORIES] = {};

        for (i = 0; i < int(wcm->plugins.size()); i++) {
                p = wcm->plugins[i];
                if (std::string(p->category) == "General")
                        add_plugin_to_category(p, &categories[0], &layout[0]);
                else if (std::string(p->category) == "Desktop")
                        add_plugin_to_category(p, &categories[1], &layout[1]);
                else if (std::string(p->category) == "Effects")
                        add_plugin_to_category(p, &categories[2], &layout[2]);
                else if (std::string(p->category) == "Window Management")
                        add_plugin_to_category(p, &categories[3], &layout[3]);
                else
                        add_plugin_to_category(p, &categories[4], &layout[4]);
        }
        for (i = 0; i < NUM_CATEGORIES; i++) {
                int add_separator = 0;
                int j;
                if (layout[i])
                        gtk_container_add(GTK_CONTAINER(plugin_buttons_layout), layout[i]);
                for (j = i + 1; j < NUM_CATEGORIES; j++)
                        add_separator |= layout[j] ? 1 : 0;
                if (add_separator) {
                        GtkWidget *separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
                        g_object_set(separator, "margin", 25, NULL);
                        gtk_container_add(GTK_CONTAINER(plugin_buttons_layout), separator);
                }
        }

        GtkWidget *bottom_spacer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_widget_set_size_request(bottom_spacer, 1, 25);
        gtk_container_add(GTK_CONTAINER(plugin_buttons_layout), bottom_spacer);
        g_signal_connect(wcm->window, "configure-event",
                         G_CALLBACK(main_panel_configure_cb), wcm);
        return plugin_buttons_layout;
}

GtkWidget *
create_main_layout(WCM *wcm)
{
        GtkWidget *window = wcm->window;

        GtkWidget *main_layout = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        GtkWidget *left_panel_layout = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);

        gtk_style_context_add_class(gtk_widget_get_style_context(scrolled_window), GTK_STYLE_CLASS_VIEW);

        GtkWidget *close_button = gtk_button_new();
        GtkWidget *close_layout = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        GtkWidget *close_image = gtk_image_new_from_icon_name("window-close", GTK_ICON_SIZE_BUTTON);
        GtkWidget *close_label = gtk_label_new("Close");
        gtk_widget_set_size_request(close_layout, 70, -1);
        gtk_widget_set_margin_start(close_layout, 70);
        gtk_widget_set_margin_end(close_layout, 70);
        gtk_box_pack_start(GTK_BOX(close_layout), close_image, true, false, 0);
        gtk_box_pack_end(GTK_BOX(close_layout), close_label, true, false, 0);
        gtk_container_add(GTK_CONTAINER(close_button), close_layout);
        g_object_set(close_button, "margin", 10, NULL);
        g_signal_connect(close_button, "button-release-event",
                         G_CALLBACK(close_button_button_cb), NULL);
        g_signal_connect(close_button, "key-press-event",
                         G_CALLBACK(close_button_key_cb), NULL);
        gtk_box_pack_end(GTK_BOX(left_panel_layout), close_button, false, false, 0);
        gtk_box_pack_start(GTK_BOX(main_layout), left_panel_layout, false, true, 0);
        gtk_container_add(GTK_CONTAINER(scrolled_window), create_plugins_layout(wcm));
        gtk_box_pack_end(GTK_BOX(main_layout), scrolled_window, true, true, 0);
        gtk_container_add(GTK_CONTAINER(window), main_layout);

        wcm->left_panel_layout = left_panel_layout;
        wcm->scrolled_plugin_layout = scrolled_window;

        return main_layout;
}

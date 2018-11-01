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

#include "wcm.h"

static int num_button_columns;

#define NUM_CATEGORIES 5

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
close_button_cb(GtkWidget *widget,
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
                        option = section->get_option(o->name, std::to_string(o->default_value.i));
                        option->set_value(o->default_value.i);
                        wcm->wf_config->save_config(wcm->config_file);
                        break;
                case OPTION_TYPE_BOOL:
                        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(o->data_widget), o->default_value.i);
                        section = wcm->wf_config->get_section(o->plugin->name);
                        option = section->get_option(o->name, std::to_string(o->default_value.i));
                        option->set_value(o->default_value.i);
                        wcm->wf_config->save_config(wcm->config_file);
                        break;
                case OPTION_TYPE_DOUBLE:
                        gtk_spin_button_set_value(GTK_SPIN_BUTTON(o->data_widget), o->default_value.d);
                        section = wcm->wf_config->get_section(o->plugin->name);
                        option = section->get_option(o->name, std::to_string(o->default_value.d));
                        option->set_value(o->default_value.d);
                        wcm->wf_config->save_config(wcm->config_file);
                        break;
                case OPTION_TYPE_BUTTON:
                case OPTION_TYPE_KEY:
                        gtk_entry_set_text(GTK_ENTRY(o->data_widget), o->default_value.s);
                        section = wcm->wf_config->get_section(o->plugin->name);
                        option = section->get_option(o->name, o->default_value.s);
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
                        option = section->get_option(o->name, o->default_value.s);
                        option->set_value(o->default_value.s);
                        wcm->wf_config->save_config(wcm->config_file);
                        break;
                case OPTION_TYPE_COLOR:
                        GdkRGBA color;
                        if (sscanf(o->default_value.s, "%lf %lf %lf %lf", &color.red, &color.green, &color.blue, &color.alpha) != 4)
                                break;
                        gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(o->data_widget), &color);
                        section = wcm->wf_config->get_section(o->plugin->name);
                        option = section->get_option(o->name, o->default_value.s);
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
        option = section->get_option(o->name, o->default_value.s);
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
        option = section->get_option(o->name, std::to_string(o->default_value.i));
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
                option = section->get_option(o->name, o->default_value.s);
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
        option = section->get_option(o->name, std::to_string(o->default_value.d));
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
        option = section->get_option(o->name, std::to_string(o->default_value.i));
        option->set_value(gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget)));
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
        option = section->get_option(o->name, std::to_string(o->default_value.i));
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
        option = section->get_option(o->name, o->default_value.s);
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
                        GtkWidget *spin_button = gtk_spin_button_new(gtk_adjustment_new(option->as_double(), o->data.min, o->data.max, o->data.precision, o->data.precision * 10, 0), o->data.precision, 3);
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
                        GtkWidget *entry = gtk_entry_new();
                        gtk_entry_set_text(GTK_ENTRY(entry), option->as_string().c_str());
                        o->data_widget = entry;
                        g_signal_connect(entry, "activate",
                                         G_CALLBACK(set_string_option_cb), o);
                        g_signal_connect(entry, "focus-out-event",
                                         G_CALLBACK(entry_focus_out_cb), o);
                        gtk_box_pack_end(GTK_BOX(option_layout), entry, true, true, 0);
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

        section = wcm->wf_config->get_section("core");
        option = section->get_option("plugins", "default");

        p->enabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

        if (p->enabled) {
                option->set_value(option->as_string() + " " + std::string(p->name));
                wcm->wf_config->save_config(wcm->config_file);
        } else {
                std::string plugins = option->as_string();
                size_t pos;
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
        GtkWidget *enable_label = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(enable_label), "<span size=\"10000\"><b>Use This Plugin</b></span>");
        GtkWidget *enabled_cb = gtk_check_button_new();
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enabled_cb), p->enabled ? true : false);
        gtk_widget_set_margin_start(enabled_cb, 50);
        gtk_widget_set_margin_end(enabled_cb, 15);
        g_signal_connect(enabled_cb, "toggled",
                         G_CALLBACK(toggle_plugin_enabled_cb), p);
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
        gtk_box_pack_start(GTK_BOX(enable_layout), enabled_cb, false, false, 0);
        gtk_box_pack_start(GTK_BOX(enable_layout), enable_label, false, false, 0);
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
                         G_CALLBACK(close_button_cb), NULL);
        gtk_box_pack_end(GTK_BOX(left_panel_layout), close_button, false, false, 0);
        gtk_box_pack_start(GTK_BOX(main_layout), left_panel_layout, false, true, 0);
        gtk_container_add(GTK_CONTAINER(scrolled_window), create_plugins_layout(wcm));
        gtk_box_pack_end(GTK_BOX(main_layout), scrolled_window, true, true, 0);
        gtk_container_add(GTK_CONTAINER(window), main_layout);

        wcm->left_panel_layout = left_panel_layout;
        wcm->scrolled_plugin_layout = scrolled_window;

        return main_layout;
}

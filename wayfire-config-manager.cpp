/*
 * g++ $(pkg-config --cflags gtk+-3.0 libxml-2.0 wf-config) -o wcm wayfire-config-manager.cpp $(pkg-config --libs gtk+-3.0 libxml-2.0 wf-config)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <gtk/gtk.h>
#include <vector>
#include <string>
#include <iomanip>
#include <sstream>
#include <config.hpp>

using namespace std;

static const char *config_file;

enum option_type
{
	OPTION_TYPE_INT,
	OPTION_TYPE_BOOL,
	OPTION_TYPE_DOUBLE,
	OPTION_TYPE_STRING,
	OPTION_TYPE_BUTTON,
	OPTION_TYPE_KEY,
	OPTION_TYPE_COLOR
};

class LabeledInt
{
	public:
	int value;
	char *name;
};

class LabeledString
{
	public:
	int id;
	char *value;
	char *name;
};

class var_data
{
	public:
	double min;
	double max;
	double precision;
};

union opt_data
{
	int i;
	char *s;
	double d;
};

class Plugin;

class Option
{
	public:
        Plugin *plugin;
	char *name;
	char *disp_name;
	option_type type;
	opt_data default_value;
	var_data data;
        vector<LabeledInt *> int_labels;
        vector<LabeledString *> str_labels;
	GtkWidget *data_widget;
};

class WCM;

class Plugin
{
        public:
        WCM *wcm;
	char *name;
	char *disp_name;
	char *category;
        vector<Option *> options;
};

class WCM
{
	public:
        GtkWidget *window;
        GtkWidget *main_layout;
        GtkWidget *plugin_layout;
	vector<Plugin *> plugins;
        wayfire_config *wf_config;
};

static int
load_config_file(WCM *wcm)
{
        char *home;

        home = getenv("HOME");
        if (!home)
		return -1;

        config_file = strdup((string(home) + "/.config/wayfire.ini").c_str());

        wcm->wf_config = new wayfire_config(config_file);

        return 0;
}

static void
get_plugin_data(Plugin *p, Option *opt, xmlDoc *doc, xmlNode * a_node)
{
	Option *o = opt;
	xmlNode *cur_node = NULL;
	xmlChar *prop;

	for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
		if (cur_node->type == XML_ELEMENT_NODE) {
			printf("element: %s\n", cur_node->name);
			if (string((char *) cur_node->name) == "plugin") {
				printf("plugin name: ");
				prop = xmlGetProp(cur_node, (xmlChar *) "name");
				if (prop) {
					printf("%s\n", prop);
					p->name = strdup((char *) prop);
				}
				free(prop);
			} else if (string((char *) cur_node->name) == "_short") {
				printf("plugin _short: ");
				printf("%s\n", cur_node->children->content);
				if (!o)
					p->disp_name = strdup((char *) cur_node->children->content);
				else
					o->disp_name = strdup((char *) cur_node->children->content);
			} else if (string((char *) cur_node->name) == "category") {
				printf("plugin category: ");
				printf("%s\n", cur_node->children->content);
				p->category = strdup((char *) cur_node->children->content);
			} else if (string((char *) cur_node->name) == "option") {
				o = new Option();
				o->plugin = p;
				printf("option name: ");
				prop = xmlGetProp(cur_node, (xmlChar *) "name");
				if (prop) {
					printf("%s\n", prop);
					o->name = strdup((char *) prop);
				}
				free(prop);
				printf("option type: ");
				prop = xmlGetProp(cur_node, (xmlChar *) "type");
				if (prop) {
					printf("%s\n", prop);
					if (string((char *) prop) == "int") {
						o->type = OPTION_TYPE_INT;
						o->data.min = DBL_MIN;
						o->data.max = DBL_MAX;
						o->data.precision = 0.1;
					} else if (string((char *) prop) == "double") {
						o->type = OPTION_TYPE_DOUBLE;
						o->data.min = DBL_MIN;
						o->data.max = DBL_MAX;
						o->data.precision = 0.1;
					} else if (string((char *) prop) == "bool") {
						o->type = OPTION_TYPE_BOOL;
					} else if (string((char *) prop) == "string") {
						o->type = OPTION_TYPE_STRING;
						o->default_value.s = strdup("");
					} else if (string((char *) prop) == "button") {
						o->type = OPTION_TYPE_BUTTON;
						o->default_value.s = strdup("");
					} else if (string((char *) prop) == "color") {
						o->type = OPTION_TYPE_COLOR;
						o->default_value.s = strdup("");
					} else if (string((char *) prop) == "key") {
						o->type = OPTION_TYPE_KEY;
						o->default_value.s = strdup("");
					} else {
						printf("WARN: [%s] unknown option type\n", p->name);
						o->type = (option_type) -1;
					}
				} else {
					printf("WARN: [%s] no option type found\n", p->name);
					o->type = (option_type) -1;
				}
				free(prop);
				p->options.push_back(o);
			} else if (string((char *) cur_node->name) == "default") {
				if (!cur_node->children)
					break;
				printf("default: %s\n", cur_node->children->content);
				switch (o->type) {
					case OPTION_TYPE_INT:
						o->default_value.i = atoi((char *) cur_node->children->content);
						break;
					case OPTION_TYPE_BOOL:
						if (string((char *) cur_node->children->content) == "true")
							o->default_value.i = 1;
						else if (string((char *) cur_node->children->content) == "false")
							o->default_value.i = 0;
						else
							o->default_value.i = atoi((char *) cur_node->children->content);
						if (o->default_value.i < 0 && o->default_value.i > 1)
							printf("WARN: [%s] unknown bool option default\n", p->name);
						break;
					case OPTION_TYPE_STRING:
					case OPTION_TYPE_BUTTON:
					case OPTION_TYPE_COLOR:
					case OPTION_TYPE_KEY:
						free(o->default_value.s);
						o->default_value.s = strdup((char *) cur_node->children->content);
						break;
					case OPTION_TYPE_DOUBLE:
						o->default_value.d = atof((char *) cur_node->children->content);
						break;
					default:
						break;
				}
			} else if (string((char *) cur_node->name) == "min") {
				if (!cur_node->children)
					break;
				if (o->type != OPTION_TYPE_INT && o->type != OPTION_TYPE_DOUBLE)
					printf("WARN: [%s] min defined for option type !int && !double\n", p->name);
				o->data.min = atof((char *) cur_node->children->content);
			} else if (string((char *) cur_node->name) == "max") {
				if (!cur_node->children)
					break;
				if (o->type != OPTION_TYPE_INT && o->type != OPTION_TYPE_DOUBLE)
					printf("WARN: [%s] max defined for option type !int && !double\n", p->name);
				o->data.max = atof((char *) cur_node->children->content);
			} else if (string((char *) cur_node->name) == "precision") {
				if (!cur_node->children)
					break;
				if (o->type != OPTION_TYPE_DOUBLE)
					printf("WARN: [%s] precision defined for option type !double\n", p->name);
				o->data.precision = atof((char *) cur_node->children->content);
			} else if (string((char *) cur_node->name) == "desc") {
				if (o->type != OPTION_TYPE_INT && o->type != OPTION_TYPE_STRING)
					printf("WARN: [%s] desc defined for option type !int && !string\n", p->name);
				xmlNode *node;
				LabeledInt *li = NULL;
				LabeledString *ls = NULL;
				for (node = cur_node->children; node; node = node->next) {
					if (node->type == XML_ELEMENT_NODE) {
						if (o->type == OPTION_TYPE_INT) {
							int is_value = (string((char *) node->name) == "value");
							int is_name = (string((char *) node->name) == "_name");
							if (!li && (is_value || is_name)) {
								li = new LabeledInt();
								o->int_labels.push_back(li);
							}
							if (is_value)
								li->value = atoi((char *) node->children->content);
							if (is_name)
								li->name = strdup((char *) node->children->content);
						} else if (o->type == OPTION_TYPE_STRING) {
							int is_value = (string((char *) node->name) == "value");
							int is_name = (string((char *) node->name) == "_name");
							if (!ls && (is_value || is_name)) {
								ls = new LabeledString();
								o->str_labels.push_back(ls);
							}
							if (is_value)
								ls->value = strdup((char *) node->children->content);
							if (is_name)
								ls->name = strdup((char *) node->children->content);
						}
					}
				}
			}
		}
		get_plugin_data(p, o, doc, cur_node->children);
	}
}

static int
parse_xml_files(WCM *wcm, const char *dir_name)
{
	int len;
	DIR *dir;
	char *name;
	struct dirent *file;
	string path, filename;
	xmlDoc *doc = NULL;
	xmlNode *root_element = NULL;

	path = string(dir_name);
	dir = opendir((path).c_str());
	if (!dir) {
		printf("Error: Could not open %s\n", (path).c_str());
		return -1;
	}
	while ((file = readdir(dir))) {
		name = file->d_name;
		len = string(name).length();
		if (len > 3 && string(&name[len - 4]) == ".xml") {
			filename = path + "/" + string(name);
			doc = xmlReadFile(filename.c_str(), NULL, 0);
			if (!doc) {
				printf("Error: Could not parse file %s\n", filename.c_str());
				continue;
			}

			root_element = xmlDocGetRootElement(doc);

			if (root_element->type == XML_ELEMENT_NODE && string((char *) root_element->name) == "wayfire") {
				printf("adding plugin: %s\n", name);
				Plugin *p = new Plugin();
				p->wcm = wcm;
				get_plugin_data(p, NULL, doc, root_element);
				wcm->plugins.push_back(p);
			}

			xmlFreeDoc(doc);
			xmlCleanupParser();
		}
	}

	closedir(dir);

	return 0;
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
			option = section->get_option(o->name, to_string(o->default_value.i));
			option->set_value(to_string(o->default_value.i));
			wcm->wf_config->save_config(config_file);
			break;
		case OPTION_TYPE_BOOL:
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(o->data_widget), o->default_value.i);
			section = wcm->wf_config->get_section(o->plugin->name);
			option = section->get_option(o->name, to_string(o->default_value.i));
			option->set_value(to_string(o->default_value.i));
			wcm->wf_config->save_config(config_file);
			break;
		case OPTION_TYPE_DOUBLE:
			gtk_spin_button_set_value(GTK_SPIN_BUTTON(o->data_widget), o->default_value.d);
			section = wcm->wf_config->get_section(o->plugin->name);
			option = section->get_option(o->name, to_string(o->default_value.d));
			option->set_value(to_string(o->default_value.d));
			wcm->wf_config->save_config(config_file);
			break;
		case OPTION_TYPE_BUTTON:
		case OPTION_TYPE_KEY:
			gtk_entry_set_text(GTK_ENTRY(o->data_widget), o->default_value.s);
			section = wcm->wf_config->get_section(o->plugin->name);
			option = section->get_option(o->name, o->default_value.s);
			option->set_value(o->default_value.s);
			wcm->wf_config->save_config(config_file);
			break;
		case OPTION_TYPE_STRING:
			if (o->str_labels.size()) {
				LabeledString *ls;
				int i;
				for (i = 0; i < int(o->str_labels.size()); i++) {
					ls = o->str_labels[i];
					if (string(ls->value) == o->default_value.s) {
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
			wcm->wf_config->save_config(config_file);
			break;
		case OPTION_TYPE_COLOR:
			GdkRGBA color;
			if (sscanf(o->default_value.s, "%lf %lf %lf %lf", &color.red, &color.green, &color.blue, &color.alpha) != 4)
				break;
			gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(o->data_widget), &color);
			section = wcm->wf_config->get_section(o->plugin->name);
			option = section->get_option(o->name, o->default_value.s);
			option->set_value(o->default_value.s);
			wcm->wf_config->save_config(config_file);
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
		wcm->wf_config->save_config(config_file);
	}
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
        option = section->get_option(o->name, to_string(o->default_value.i));
        option->set_value(to_string(int(gtk_combo_box_get_active(GTK_COMBO_BOX(widget)))));
        wcm->wf_config->save_config(config_file);
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

	GtkWidget *chooser = gtk_color_chooser_dialog_new("Pick a Color", GTK_WINDOW(o->plugin->wcm->window));
	gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(widget), &color);
	gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(chooser), &color);

	if (gtk_dialog_run(GTK_DIALOG(chooser)) == GTK_RESPONSE_OK) {
		gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(chooser), &color);
		gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(widget), &color);
		section = wcm->wf_config->get_section(o->plugin->name);
		option = section->get_option(o->name, to_string(o->default_value.d));
                stringstream stream;
                stream << fixed << setprecision(1) << color.red << " " << color.green << " " << color.blue << " " << color.alpha;
                string color_str = stream.str();
		option->set_value(color_str);
		wcm->wf_config->save_config(config_file);
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
        option = section->get_option(o->name, to_string(o->default_value.d));
        option->set_value(to_string(gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget))));
        wcm->wf_config->save_config(config_file);
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
        option = section->get_option(o->name, to_string(o->default_value.i));
        option->set_value(to_string(int(gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget)))));
        wcm->wf_config->save_config(config_file);
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
        option = section->get_option(o->name, to_string(o->default_value.i));
        option->set_value(to_string(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)) ? 1 : 0));
        wcm->wf_config->save_config(config_file);
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
        wcm->wf_config->save_config(config_file);
}

static gboolean
entry_focus_out_cb(GtkWidget *widget,
                   GdkEventButton *event,
                   gpointer user_data)
{
	set_string_option_cb(widget, user_data);

	return true;
}

static void
add_option_widget(GtkWidget *widget, Option *o)
{
	WCM *wcm = o->plugin->wcm;
	GtkWidget *option_layout, *label, *reset_button, *reset_image;
        wayfire_config_section *section;
        wf_option option;

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
			gtk_widget_set_size_request(label, 200, 1);
			gtk_label_set_xalign(GTK_LABEL(label), 0);
			reset_button = gtk_button_new();
			gtk_widget_set_margin_start(reset_button, 10);
			gtk_widget_set_margin_end(reset_button, 10);
			gtk_widget_set_margin_top(reset_button, 10);
			gtk_widget_set_tooltip_text(reset_button, "Reset to default");
			g_signal_connect(reset_button, "button-release-event",
					 G_CALLBACK(reset_button_cb), o);
			reset_image = gtk_image_new_from_icon_name("edit-clear", GTK_ICON_SIZE_BUTTON);
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
			option = section->get_option(o->name, to_string(o->default_value.i));
			GtkWidget *combo_box;
			GtkWidget *spin_button;
			if (o->int_labels.size()) {
				combo_box = gtk_combo_box_text_new();
				gtk_widget_set_margin_top(combo_box, 10);
				for (i = 0; i < int(o->int_labels.size()); i++) {
					li = o->int_labels[i];
					gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_box), li->name);
				}
				gtk_combo_box_set_active(GTK_COMBO_BOX(combo_box), o->default_value.i);
				o->data_widget = combo_box;
				g_signal_connect(combo_box, "changed",
						 G_CALLBACK(set_int_combo_box_option_cb), o);
				gtk_box_pack_end(GTK_BOX(option_layout), combo_box, true, true, 0);
			} else {
				spin_button = gtk_spin_button_new(gtk_adjustment_new(option->as_int(), o->data.min, o->data.max, 1, 10, 10), 1, 0);
				gtk_widget_set_margin_top(spin_button, 10);
				o->data_widget = spin_button;
				g_signal_connect(spin_button, "changed",
						 G_CALLBACK(set_int_spin_button_option_cb), o);
				gtk_box_pack_end(GTK_BOX(option_layout), spin_button, false, true, 0);
			}
			gtk_box_pack_end(GTK_BOX(widget), option_layout, true, true, 0);
		}
			break;
		case OPTION_TYPE_BOOL: {
			option = section->get_option(o->name, to_string(o->default_value.i));
			GtkWidget *check_button = gtk_check_button_new();
			gtk_widget_set_margin_top(check_button, 10);
			section = wcm->wf_config->get_section(o->plugin->name);
			option = section->get_option(o->name, to_string(o->default_value.i));
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_button), o->default_value.i ? 1 : 0);
			o->data_widget = check_button;
			g_signal_connect(check_button, "toggled",
					 G_CALLBACK(set_bool_check_button_option_cb), o);
			gtk_box_pack_end(GTK_BOX(option_layout), check_button, false, true, 0);
			gtk_box_pack_end(GTK_BOX(widget), option_layout, true, true, 0);
		}
			break;
		case OPTION_TYPE_DOUBLE: {
			option = section->get_option(o->name, to_string(o->default_value.d));
			GtkWidget *spin_button = gtk_spin_button_new(gtk_adjustment_new(option->as_double(), o->data.min, o->data.max, o->data.precision, o->data.precision * 10, o->data.precision * 10), o->data.precision, 3);
			gtk_widget_set_margin_top(spin_button, 10);
			o->data_widget = spin_button;
			g_signal_connect(spin_button, "changed",
					 G_CALLBACK(set_double_spin_button_option_cb), o);
			gtk_box_pack_end(GTK_BOX(option_layout), spin_button, false, true, 0);
			gtk_box_pack_end(GTK_BOX(widget), option_layout, true, true, 0);
		}
			break;
		case OPTION_TYPE_BUTTON:
		case OPTION_TYPE_KEY: {
			option = section->get_option(o->name, o->default_value.s);
			GtkWidget *entry = gtk_entry_new();
			gtk_widget_set_margin_top(entry, 10);
			gtk_entry_set_text(GTK_ENTRY(entry), option->as_string().c_str());
			o->data_widget = entry;
			g_signal_connect(entry, "activate",
					 G_CALLBACK(set_string_option_cb), o);
			g_signal_connect(entry, "focus-out-event",
					 G_CALLBACK(entry_focus_out_cb), o);
			gtk_box_pack_end(GTK_BOX(option_layout), entry, true, true, 0);
			gtk_box_pack_end(GTK_BOX(widget), option_layout, true, true, 0);
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
				gtk_widget_set_margin_top(combo_box, 10);
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
				gtk_box_pack_end(GTK_BOX(option_layout), combo_box, true, true, 0);
			} else {
				entry = gtk_entry_new();
				gtk_widget_set_margin_top(entry, 10);
				gtk_entry_set_text(GTK_ENTRY(entry), option->as_string().c_str());
				o->data_widget = entry;
				g_signal_connect(entry, "activate",
						 G_CALLBACK(set_string_option_cb), o);
				g_signal_connect(entry, "focus-out-event",
						 G_CALLBACK(entry_focus_out_cb), o);
				gtk_box_pack_end(GTK_BOX(option_layout), entry, true, true, 0);
			}
			gtk_box_pack_end(GTK_BOX(widget), option_layout, true, true, 0);
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
			gtk_widget_set_margin_top(color_button, 10);
			o->data_widget = color_button;
			g_signal_connect(color_button, "button-release-event",
					 G_CALLBACK(spawn_color_chooser_cb), o);
			gtk_box_pack_end(GTK_BOX(option_layout), color_button, false, false, 0);
			gtk_box_pack_end(GTK_BOX(widget), option_layout, true, true, 0);
		}
			break;
		default:
			break;
	}
}

static void
toggle_enabled_cb(GtkWidget *widget,
                  gpointer user_data)
{
	Plugin *p = (Plugin *) user_data;
	printf("%s plugin %s\n", p->name, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)) ? "enabled" : "disabled");
}

static gboolean
plugin_button_cb(GtkWidget *widget,
                 GdkEventButton *event,
                 gpointer user_data)
{
	int i;
	Plugin *p = (Plugin *) user_data;
	WCM *wcm = p->wcm;
        GtkWidget *window = wcm->window;
        GtkWidget *main_layout = wcm->main_layout;

	g_object_ref(main_layout);
        gtk_container_remove(GTK_CONTAINER(window), main_layout);

	GtkWidget *plugin_layout = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	GtkWidget *left_panel_layout = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_widget_set_size_request(left_panel_layout, 250, 1);
	GtkWidget *main_panel_layout = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        GtkWidget *label = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(label), ("<span size=\"12000\"><b>" + string(p->disp_name) + "</b></span>").c_str());
	g_object_set(label, "margin", 50, NULL);
	GtkWidget *enable_layout = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        GtkWidget *enable_label = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(enable_label), "<span size=\"10000\"><b>Use This Plugin</b></span>");
	GtkWidget *enabled_cb = gtk_check_button_new();
	gtk_widget_set_margin_start(enabled_cb, 50);
	gtk_widget_set_margin_end(enabled_cb, 15);
        g_signal_connect(enabled_cb, "toggled",
                         G_CALLBACK(toggle_enabled_cb), p);
        GtkWidget *back_button = gtk_button_new_with_label("Back");
	GtkWidget *back_image = gtk_image_new_from_icon_name("back", GTK_ICON_SIZE_BUTTON);
	gtk_button_set_image(GTK_BUTTON(back_button), back_image);
	g_object_set(back_button, "margin", 10, NULL);
        g_signal_connect(back_button, "button-release-event",
                         G_CALLBACK(back_button_cb), wcm);
	GtkWidget *notebook = gtk_notebook_new();
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(notebook), GTK_POS_TOP);
	GtkWidget *options_layout = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	for (i = int(p->options.size()) - 1; i >= 0; i--) {
		add_option_widget(options_layout, p->options[i]);
	}
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), options_layout, gtk_label_new("General"));
	gtk_box_pack_start(GTK_BOX(main_panel_layout), notebook, false, true, 10);
	gtk_box_pack_start(GTK_BOX(left_panel_layout), label, false, false, 0);
	gtk_box_pack_start(GTK_BOX(enable_layout), enabled_cb, false, false, 0);
	gtk_box_pack_start(GTK_BOX(enable_layout), enable_label, false, false, 0);
	gtk_box_pack_start(GTK_BOX(left_panel_layout), enable_layout, false, false, 0);
	gtk_box_pack_end(GTK_BOX(left_panel_layout), back_button, false, false, 0);
	gtk_box_pack_start(GTK_BOX(plugin_layout), left_panel_layout, false, true, 0);
	gtk_box_pack_end(GTK_BOX(plugin_layout), main_panel_layout, true, true, 0);
        gtk_container_add(GTK_CONTAINER(window), plugin_layout);

        wcm->plugin_layout = plugin_layout;

        gtk_widget_show_all(window);

        return true;
}

static GtkWidget *
create_main_layout(WCM *wcm)
{
	int i;
	Plugin *p;
        GtkWidget *window = wcm->window;

	GtkWidget *main_layout = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	GtkWidget *left_panel_layout = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_widget_set_size_request(left_panel_layout, 250, 0);
	GtkWidget *main_panel_layout = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

	GtkCssProvider* provider = gtk_css_provider_new();
	GdkDisplay* display = gdk_display_get_default();
	GdkScreen* screen = gdk_display_get_default_screen(display);
	gtk_style_context_add_provider_for_screen(screen,
						  GTK_STYLE_PROVIDER(provider),
						  GTK_STYLE_PROVIDER_PRIORITY_USER);
	gtk_css_provider_load_from_data(GTK_CSS_PROVIDER(provider),
					"#main_panel_layout { background-color: white; }", -1, NULL);
	g_object_unref(provider);
	gtk_widget_set_name(main_panel_layout, "main_panel_layout");

	for (i = int(wcm->plugins.size()) - 1; i >= 0; i--) {
		p = wcm->plugins[i];
		GtkWidget *plugin_button = gtk_button_new_with_label(p->disp_name);
		g_object_set(plugin_button, "margin", 10, NULL);
		gtk_box_pack_start(GTK_BOX(main_panel_layout), plugin_button, false, true, 0);
		g_signal_connect(plugin_button, "button-release-event",
				 G_CALLBACK(plugin_button_cb), p);
	}
	GtkWidget *close_button = gtk_button_new_with_label("Close");
	GtkWidget *close_image = gtk_image_new_from_icon_name("window-close", GTK_ICON_SIZE_BUTTON);
	gtk_button_set_image(GTK_BUTTON(close_button), close_image);
	g_object_set(close_button, "margin", 10, NULL);
        g_signal_connect(close_button, "button-release-event",
                         G_CALLBACK(close_button_cb), NULL);
	gtk_box_pack_end(GTK_BOX(left_panel_layout), close_button, false, false, 0);
	gtk_box_pack_start(GTK_BOX(main_layout), left_panel_layout, false, true, 0);
	gtk_box_pack_end(GTK_BOX(main_layout), main_panel_layout, true, true, 0);
        gtk_container_add(GTK_CONTAINER(window), main_layout);

        return main_layout;
}

static void
activate(GtkApplication* app,
         gpointer user_data)
{
	WCM *wcm = (WCM *) user_data;
        GtkWidget *window;

        window = gtk_application_window_new(app);
        gtk_widget_set_size_request(window, 750, 500);
        gtk_window_set_default_size(GTK_WINDOW(window), 1000, 580);

        wcm->window = window;

        wcm->main_layout = create_main_layout(wcm);

        gtk_window_set_title(GTK_WINDOW(window), "Wayfire Config Manager");
        gtk_widget_show_all(window);
}

int
main(int argc, char **argv)
{
        GtkApplication *app;
        int status;
	WCM *wcm;
	int i, j;
	Plugin *p;
	Option *o;

	wcm = new WCM();

	if (parse_xml_files(wcm, "./metadata"))
		return -1;

	if (load_config_file(wcm))
		return -1;

	printf("*******************************\n");

	for (i = 0; i < int(wcm->plugins.size()); i++) {
		p = wcm->plugins[i];
		printf("Plugin:\n");
		printf("%s: %s\n", p->name, p->disp_name);
		printf("Options:\n");
		for (j = 0; j < int(p->options.size()); j++) {
			o = p->options[j];
			printf("%s: %s\n", o->name, o->disp_name);
		}
	}

        app = gtk_application_new("org.gtk.wayfire-config-manager", G_APPLICATION_FLAGS_NONE);
        g_signal_connect(app, "activate", G_CALLBACK(activate), wcm);
        status = g_application_run(G_APPLICATION(app), argc, argv);
        g_object_unref(app);

	delete wcm->wf_config;
	delete wcm;

	return status;
}

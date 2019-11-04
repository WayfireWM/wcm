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

#include <vector>
#include <algorithm>
#include <string.h>
#include <gtk/gtk.h>
#include <config.hpp>
#include <wlr-input-inhibitor-unstable-v1-client-protocol.h>

enum option_type
{
        OPTION_TYPE_INT,
        OPTION_TYPE_BOOL,
        OPTION_TYPE_DOUBLE,
        OPTION_TYPE_STRING,
        OPTION_TYPE_BUTTON,
        OPTION_TYPE_KEY,
        OPTION_TYPE_COLOR,
        OPTION_TYPE_GROUP,
        OPTION_TYPE_SUBGROUP,
        OPTION_TYPE_DYNAMIC_LIST
};

enum mod_type
{
        MOD_TYPE_SHIFT   = 1 << 0,
        MOD_TYPE_CONTROL = 1 << 1,
        MOD_TYPE_ALT     = 1 << 2,
        MOD_TYPE_SUPER   = 1 << 3
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
        char *binding;
        option_type type;
        mod_type mod_mask;
        opt_data default_value;
        var_data data;
        Option *parent;
        GtkWidget *widget;
        GtkWidget *data_widget;
        GtkWidget *label_widget;
        GtkWidget *aux_window;
        GtkWidget *confirm_window;
        GtkWidget *command_combo;
        GtkWidget *binding_entry;
        GtkWidget *command_expander;
        std::vector<Option *> options;
        std::vector<LabeledInt *> int_labels;
        std::vector<LabeledString *> str_labels;
};

class WCM;

class Plugin
{
        public:
        WCM *wcm;
        char *name;
        char *disp_name;
        char *category;
        int x, y;
        int enabled;
        GtkWidget *t1, *t2;
        std::vector<Option *> options;
};

class WCM
{
        public:
        GtkWidget *window;
        GtkWidget *main_layout;
        GtkWidget *plugin_layout;
        GtkWidget *left_panel_layout;
        GtkWidget *scrolled_plugin_layout;
        std::vector<Plugin *> plugins;
        wayfire_config *wf_config;
        const char *config_file;
        zwlr_input_inhibitor_v1 *screen_lock;
        zwlr_input_inhibit_manager_v1 *inhibitor_manager;
};

int
load_config_file(WCM *wcm);

int
parse_xml_files(WCM *wcm, const char *dir_name);

GtkWidget *
create_main_layout(WCM *wcm);

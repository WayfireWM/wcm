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
 * 
 * meson build --prefix=/usr && ninja -C build && sudo ninja -C build install
 * 
 */

#include <string.h>
#include "wcm.h"

static int
load_config_file(WCM *wcm)
{
        const gchar *config_dir;

        config_dir = g_get_user_config_dir();

        wcm->config_file = strdup((std::string(config_dir) + "/wayfire.ini").c_str());

        wcm->wf_config = new wayfire_config(wcm->config_file);

        return 0;
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

        wcm = new WCM();

        if (load_config_file(wcm))
                return -1;

        if (parse_xml_files(wcm, METADATADIR))
                return -1;

        init(wcm);

        app = gtk_application_new("org.gtk.wayfire-config-manager", G_APPLICATION_FLAGS_NONE);
        g_signal_connect(app, "activate", G_CALLBACK(activate), wcm);
        status = g_application_run(G_APPLICATION(app), argc, argv);
        g_object_unref(app);

        delete wcm->wf_config;
        delete wcm;

        return status;
}

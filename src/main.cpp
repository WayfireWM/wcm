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

#include <sstream>
#include <wordexp.h>
#include "wcm.hpp"

int load_config_files(WCM *wcm)
{
    wordexp_t exp;
    char *wf_config_file_override = getenv("WAYFIRE_CONFIG_FILE");
    char *wf_shell_config_file_override = getenv("WF_SHELL_CONFIG_FILE");

    if (wf_config_file_override)
    {
        wcm->wf_config_file = wf_config_file_override;
    } else
    {
        wordexp(WAYFIRE_CONFIG_FILE, &exp, 0);
        wcm->wf_config_file = strdup(exp.we_wordv[0]);
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

    wcm->wf_config_mgr = wf::config::build_configuration(wayfire_xmldirs,
        WAYFIRE_SYSCONFDIR "/wayfire/defaults.ini", wcm->wf_config_file);

    if (wf_shell_config_file_override)
    {
        wcm->wf_shell_config_file = wf_shell_config_file_override;
    } else
    {
        wordexp(WF_SHELL_CONFIG_FILE, &exp, 0);
        wcm->wf_shell_config_file = strdup(exp.we_wordv[0]);
        wordfree(&exp);
    }

#if HAVE_WFSHELL
    std::vector<std::string> wf_shell_xmldirs(1, WFSHELL_METADATADIR);
    wcm->wf_shell_config_mgr = wf::config::build_configuration(wf_shell_xmldirs,
        WFSHELL_SYSCONFDIR "/wayfire/wf-shell-defaults.ini",
        wcm->wf_shell_config_file);
#endif

    return 0;
}

static void registry_add_object(void *data, struct wl_registry *registry,
    uint32_t name, const char *interface, uint32_t version)
{
    WCM *wcm = (WCM*)data;

    if (strcmp(interface, zwlr_input_inhibit_manager_v1_interface.name) == 0)
    {
        wcm->inhibitor_manager = (zwlr_input_inhibit_manager_v1*)wl_registry_bind(
            registry, name, &zwlr_input_inhibit_manager_v1_interface, 1u);
    }
}

static void registry_remove_object(void *data, struct wl_registry *registry,
    uint32_t name)
{}

static struct wl_registry_listener registry_listener =
{
    &registry_add_object,
    &registry_remove_object
};

static bool init_input_inhibitor(WCM *wcm)
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

    wl_registry_add_listener(registry, &registry_listener, wcm);
    wl_display_dispatch(display);
    wl_display_roundtrip(display);
    if (!wcm->inhibitor_manager)
    {
        std::cerr << "Compositor does not support " <<
            "wlr_input_inhibit_manager_v1" << std::endl;

        return false;
    }

    return true;
}

bool is_core_plugin(Plugin *p)
{
    if ((std::string(p->name) == "core") ||
        (std::string(p->name) == "input") ||
        (std::string(p->name) == "workarounds"))
    {
        return true;
    }

    return false;
}

static void activate(GtkApplication *app,
    gpointer user_data)
{
    WCM *wcm = (WCM*)user_data;
    GtkWidget *window;
    GdkPixbuf *icon;

    window = gtk_application_window_new(app);
    icon   = gdk_pixbuf_new_from_file(ICONDIR "/wcm.png", NULL);
    gtk_widget_set_size_request(window, 750, 500);
    gtk_window_set_default_size(GTK_WINDOW(window), 1000, 580);
    gtk_window_set_icon(GTK_WINDOW(window), icon);

    wcm->window = window;

    wcm->main_layout = create_main_layout(wcm);

    gtk_window_set_title(GTK_WINDOW(window), "Wayfire Config Manager");
    gtk_widget_show_all(window);

    if (!init_input_inhibitor(wcm))
    {
        std::cerr << "binding grabs will not work" << std::endl;
    }
}

static int plugin_enabled(Plugin *p, std::string plugins)
{
    char c1, c2;
    std::string::size_type pos;

    if (is_core_plugin(p))
    {
        return 1;
    }

    pos = plugins.find(std::string(p->name));

    if (!pos)
    {
        return 1;
    }

    if (pos == std::string::npos)
    {
        return 0;
    }

    c1 = plugins[pos - 1];
    c2 = plugins[pos + strlen(p->name)];

    return (c1 == ' ' || c1 == 0) && (c2 == ' ' || c2 == 0);
}

static void init(WCM *wcm)
{
    Plugin *p;
    int i;

    auto section = wcm->wf_config_mgr.get_section("core");
    auto option  = section->get_option("plugins");
    auto plugins = option->get_value_str();

    for (i = 0; i < int(wcm->plugins.size()); i++)
    {
        p = wcm->plugins[i];
        p->enabled = plugin_enabled(p, plugins);
    }
}

int main(int argc, char **argv)
{
    GtkApplication *app;
    int status;
    WCM *wcm;

    wcm = new WCM();

    if (load_config_files(wcm))
    {
        return -1;
    }

    if (parse_xml_files(wcm, &wcm->wf_config_mgr))
    {
        return -1;
    }

#if HAVE_WFSHELL
    if (parse_xml_files(wcm, &wcm->wf_shell_config_mgr))
    {
        return -1;
    }

#endif

    init(wcm);

    app = gtk_application_new("org.gtk.wayfire-config-manager",
        G_APPLICATION_FLAGS_NONE);
    g_signal_connect(app, "activate", G_CALLBACK(activate), wcm);
    status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);

    delete wcm;

    return status;
}

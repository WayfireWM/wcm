#include <libintl.h>
#include <locale.h>
#include <wcm.hpp>

int main(int argc, char **argv)
{
    setlocale(LC_ALL, "");
    bindtextdomain("wcm", WAYFIRE_LOCALEDIR);
    textdomain("wcm");
    auto app = Gtk::Application::create("org.gtk.wcm");
    std::unique_ptr<WCM> wcm = std::make_unique<WCM>(app);
    return app->run(argc, argv);
}

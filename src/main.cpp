#include <wcm.hpp>

int main(int argc, char **argv)
{
    auto app = Gtk::Application::create("org.gtk.wayfire-config-manager");
    std::unique_ptr<WCM> wcm;
    app->signal_startup().connect([&] { wcm = std::make_unique<WCM>(app); });
    return app->run(argc, argv);
}

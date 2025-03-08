#include <wcm.hpp>

int main(int argc, char **argv)
{
    auto app = Gtk::Application::create("org.gtk.wcm");
    std::unique_ptr<WCM> wcm = std::make_unique<WCM>(app);
    return app->run(argc, argv);
}

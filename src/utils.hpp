#ifndef UTILS_HPP
#define UTILS_HPP

#include <gtkmm.h>
#include <string>

/*!
 * Simple fuzzy-search. Finds `pattern` in `text`. Always returns `true` when `pattern` is empty.
 */
bool find_string(std::string text, std::string pattern);

/*!
 * Button with text and icon.
 */
class PrettyButton : public Gtk::Button
{
    public:
    PrettyButton(const Glib::ustring &text, const Glib::ustring &icon, Gtk::IconSize icon_size = Gtk::ICON_SIZE_BUTTON)
        : label(text)
    {
        image.set_from_icon_name(icon, icon_size);
        layout.pack_start(image);
        layout.pack_start(label);
        layout.set_halign(Gtk::ALIGN_CENTER);
        add(layout);
    }

    private:
    Gtk::Box layout = Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 5);
    Gtk::Image image;
    Gtk::Label label;
};

#endif

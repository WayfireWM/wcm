#pragma once

#include <gtkmm.h>
#include <string>
#include <wayfire/config/section.hpp>

using wf_section = std::shared_ptr<wf::config::section_t>;

/*!
 * Simple fuzzy-search. Finds `pattern` in `text`. Always returns `true` when
 * `pattern` is empty.
 */
bool find_string(std::string text, std::string pattern);

bool begins_with(const std::string & str, const std::string & prefix);

/*!
 * Button with text and icon.
 */
class PrettyButton : public Gtk::Button
{
  public:
    PrettyButton(const Glib::ustring & text, const Glib::ustring & icon,
        Gtk::IconSize icon_size = Gtk::ICON_SIZE_BUTTON) :
        label(text)
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

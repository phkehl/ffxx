/* ****************************************************************************************************************** */
// flipflip's gui (ffgui)
//
// Copyright (c) Philippe Kehl (flipflip at oinkzwurgl dot org)
// https://oinkzwurgl.org/projaeggd/ffxx/
//
// This program is free software: you can redistribute it and/or modify it under the terms of the
// GNU General Public License as published by the Free Software Foundation, either version 3 of the
// License.
//
// This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
// even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along with this program.
// If not, see <https://www.gnu.org/licenses/>.

#ifndef __GUI_WIDGET_TABBAR_HPP__
#define __GUI_WIDGET_TABBAR_HPP__

//
#include "ffgui_inc.hpp"
//
#include <functional>

namespace ffgui {
/* ****************************************************************************************************************** */

class GuiWidgetTabbar
{
   public:
    GuiWidgetTabbar(const std::string& name, const int flags = ImGuiTabBarFlags_None);
    virtual ~GuiWidgetTabbar();

    bool Begin();
    void Item(const std::string& label, std::function<void(void)> cb);
    bool Item(const std::string& label, bool& changed, bool* close = nullptr, const ImGuiTabItemFlags flags = ImGuiTabItemFlags_None);
    bool Item(const std::string& label, bool* close = nullptr, const ImGuiTabItemFlags flags = ImGuiTabItemFlags_None);
    void Item(const std::string& label, bool& changed, bool& selected);
    void End();

    void Switch(const std::string& name);

   private:
    std::string name_;
    struct Config
    {
        std::string selected;
    };
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Config, selected);
    Config cfg_;
    int tabbarFlags_ = 0;
    std::string setSelected_;
    std::string currTab_;
};

/* ****************************************************************************************************************** */
}  // namespace ffgui
#endif  // __GUI_WIDGET_TABBAR_HPP__

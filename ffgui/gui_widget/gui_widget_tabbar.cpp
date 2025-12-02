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

//
#include "ffgui_inc.hpp"
//
#include "gui_widget_tabbar.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

GuiWidgetTabbar::GuiWidgetTabbar(const std::string& name, const int flags) /* clang-format off */ :
    name_        { name },
    tabbarFlags_ { flags }  // clang-format on
{
    GuiGlobal::LoadObj(name_ + ".GuiWidgetTabbar", cfg_);
    setSelected_ = cfg_.selected;
}

GuiWidgetTabbar::~GuiWidgetTabbar()
{
    GuiGlobal::SaveObj(name_ + ".GuiWidgetTabbar", cfg_);
}

// ---------------------------------------------------------------------------------------------------------------------

bool GuiWidgetTabbar::Begin()
{
    return ImGui::BeginTabBar(name_.c_str(), tabbarFlags_);
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWidgetTabbar::End()
{
    setSelected_.clear();
    ImGui::EndTabBar();
}

// ---------------------------------------------------------------------------------------------------------------------

bool GuiWidgetTabbar::Item(const std::string& label, bool& changed, bool* close, const ImGuiTabItemFlags flags)
{
    bool res = false;
    auto hashPos = label.rfind('#');
    ImGuiTabItemFlags tflags = flags;
    if (!setSelected_.empty() &&
        ((((hashPos == std::string::npos) && (label == setSelected_)) ||
            ((hashPos != std::string::npos) && (label.substr(hashPos + 1) == setSelected_))))) {
        setSelected_.clear();
        tflags |= ImGuiTabItemFlags_SetSelected;
    }

    bool open = true;
    if ((close ? ImGui::BeginTabItem(label.c_str(), &open, tflags)
               : ImGui::BeginTabItem(label.c_str(), nullptr, tflags))) {
        if (cfg_.selected != label) {
            cfg_.selected = (hashPos == std::string::npos ? label : label.substr(hashPos + 1));
        }
        if (cfg_.selected != currTab_) {
            currTab_ = cfg_.selected;
            changed = true;
        }
        res = true;
        ImGui::EndTabItem();
    }

    if (close && !open) {
        *close = true;
    }

    return res;
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWidgetTabbar::Item(const std::string& label, std::function<void(void)> cb)
{
    bool dummy;
    if (Item(label, dummy)) {
        cb();
    }
}

bool GuiWidgetTabbar::Item(const std::string& label, bool* close, const ImGuiTabItemFlags flags)
{
    bool dummy;
    return Item(label, dummy, close, flags);
}

void GuiWidgetTabbar::Item(const std::string& label, bool& changed, bool& selected)
{
    selected = Item(label, changed);
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWidgetTabbar::Switch(const std::string& name)
{
    setSelected_ = name;
}

/* ****************************************************************************************************************** */
}  // namespace ffgui

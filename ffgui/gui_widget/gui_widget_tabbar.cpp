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

GuiWidgetTabbar::State GuiWidgetTabbar::Item(const std::string& label, std::function<void(const State&)> cb, const bool allowClose, const ImGuiTabItemFlags flags)
{
    State state;
    ImGuiTabItemFlags tflags = flags;
    auto hashPos = label.rfind('#');
    const std::string name = (hashPos == std::string::npos ? label : label.substr(hashPos + 1));

    // Switch tab programatically?
    if (!setSelected_.empty() && (name == setSelected_)) {
        setSelected_.clear();
        tflags |= ImGuiTabItemFlags_SetSelected;
    }

    // Start tab
    bool open = true;
    state.isSelected_ = ImGui::BeginTabItem(label.c_str(), allowClose ? &open : nullptr, tflags);
    state.gotClosed_ = !open;

    // Tab is selected (open)
    if (state.isSelected_) {

        // Different tab now?
        if (cfg_.selected != name) {
            state.newlySelected_ = true;
            cfg_.selected = name;
        }

        // Render now?
        if (cb) {
            cb(state);
        }

        ImGui::EndTabItem();
    }

    if (state.newlySelected_ || state.gotClosed_) {
        TRACE("GuiWidgetTabbar %s newlySelected=%s gotClosed=%s", name.c_str(), ToStr(state.newlySelected_), ToStr(state.gotClosed_));
    }

    return state;
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWidgetTabbar::Switch(const std::string& name)
{
    setSelected_ = name;
}

/* ****************************************************************************************************************** */
}  // namespace ffgui

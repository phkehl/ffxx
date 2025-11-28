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
#include "input.hpp"
//

namespace ffgui {
/* ****************************************************************************************************************** */

static InputPtrMap gInputs;

InputPtr CreateInput(const std::string& name, const DataSrcDst srcId, const InputType type)
{
    DEBUG("CreateInput %p (%s)", srcId, name.c_str());
    auto input = std::make_shared<Input>(name, srcId, type);
    gInputs[name] = input;
    return input;
}

void RemoveInput(const std::string& name)
{
    auto entry = gInputs.find(name);
    if (entry != gInputs.end()) {
        DEBUG("RemoveInput %p (%s)", entry->second->srcId_, entry->second->name_.c_str());
        gInputs.erase(entry);
    }
}

const InputPtrMap& GetInputs()
{
    return gInputs;
}

// ---------------------------------------------------------------------------------------------------------------------

void DrawInputDebug()
{
    Gui::TextTitle("Input");
    for (const auto& [ name, input ] : gInputs) {
        ImGui::Text("%" PRIuMAX " %p (%s)", input.use_count(), input->srcId_, input->name_.c_str());
    }
}

/* ****************************************************************************************************************** */
}  // namespace ffgui

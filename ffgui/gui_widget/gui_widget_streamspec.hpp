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

#ifndef __GUI_WIDGET_STREAMSPEC_HPP__
#define __GUI_WIDGET_STREAMSPEC_HPP__

//
#include "ffgui_inc.hpp"
//
#include <functional>
#include <list>
#include <optional>
//
#include <ffxx/stream.hpp>
using namespace ffxx;
//
#include "gui_widget_history.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

class GuiWidgetStreamSpec
{
   public:
    using OnBaudrateFn = std::function<void(const uint32_t baudrate)>;

    GuiWidgetStreamSpec(const std::string& name, HistoryEntries& history, OnBaudrateFn onBaudrateFn = nullptr);
    virtual ~GuiWidgetStreamSpec();

    bool DrawWidget(const bool disabled = false, const bool allowBaudrate = true, const float width = -FLT_MIN);

    // clang-format off
    void               Focus(const bool selectAll = true);
    void               Clear();
    const std::string& GetSpec() const                                          { return cfg_.spec; }
    void               SetSpec(const std::string& data);
    bool               SpecOk() const                                           { return ok_; }
    uint32_t           Baudrate() const                                         { return baudrate_; }
    AutobaudMode       AbMode() const                                           { return abMode_; }
    uint32_t           SetBaudrate(const uint32_t baudrate);
    AutobaudMode       SetAbMode(const AutobaudMode abMode);
    // clang-format on

    void AddHistory(const std::string& spec);

   private:
    struct Config
    {
        std::string spec;
    };
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Config, spec)
    Config cfg_;

    std::string name_;
    std::string id_;
    GuiWidgetHistory history_;
    OnBaudrateFn onBaudrateFn_;
    std::string error_;
    bool focus_ = false;
    bool selectAll_ = false;
    StreamOptsPtr opts_;
    bool ok_ = false;
    uint32_t baudrate_ = 0;
    AutobaudMode abMode_ = AutobaudMode::NONE;
    void _Update();

    std::optional<std::string> _AddHistoryEntries();
    EnumeratedPorts enumeratedPorts_;

    static constexpr std::array<const char*, 4> STREAM_EXAMPLES = { {
        "serial://device:921600:auto",
        "tcpcli://localhost:12345,R=2,C=2",
        "ntripcli://user:pass@localhost:12345/MP:auto,R=2,C=2",
        "tcpsvr://:12345",
    } };
};

/* ****************************************************************************************************************** */
}  // namespace ffgui
#endif  // __GUI_WIDGET_STREAMSPEC_HPP__

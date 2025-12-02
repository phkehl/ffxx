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

#ifndef __GUI_WIN_DATA_MESSAGES_HPP__
#define __GUI_WIN_DATA_MESSAGES_HPP__

//
#include "ffgui_inc.hpp"
//
#include <ubloxcfg/ubloxcfg.h>
#include <fpsdk_common/parser/types.hpp>
using namespace fpsdk::common::parser;
//
#include "gui_widget_hexdump.hpp"
#include "gui_win_data.hpp"
#include "gui_msg.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

class GuiWinDataMessages : public GuiWinData
{
   public:
    GuiWinDataMessages(const std::string& name, const InputPtr& input);
    ~GuiWinDataMessages();

   private:
    struct Config
    {
        bool showList = true;
        bool showHexDump = true;
        std::string selectedMessage;
    };
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Config, showList, showHexDump, selectedMessage)
    Config cfg_;

    void _ProcessData(const DataPtr& data) final;
    void _Loop(const Time& now) final;
    void _ClearData() final;

    void _DrawToolbar() final;
    void _DrawContent() final;
    void _DrawList();
    void _DrawMessage();
    void _DrawMsgConfMenu();

    struct MsgInfo
    {
        MsgInfo() = delete;
        MsgInfo(const std::string& msgName, const std::string& winName, std::unique_ptr<GuiMsg> guiMsg);
        void Reset();
        void Update(const DataMsgPtr& msg);
        void Update(const Time& now);

        std::string name_;
        std::size_t nameId_ = 0;
        std::string group_;
        std::size_t groupId_ = 0;
        std::string imguiId_;

        GuiWidgetHexdump hexdump_;

        std::size_t count_ = 0;
        std::size_t bytes_ = 0;

        float msgRate_ = 0.0f;
        float byteRate_ = 0.0f;
        float age_ = 0.0f;

        std::string countStr_;
        std::string bytesStr_;
        std::string msgRateStr_;
        std::string byteRateStr_;
        std::string ageStr_;

        bool recent_ = false;
        DataMsgPtr msg_;

        std::unique_ptr<GuiMsg> guiMsg_;

        inline bool operator<(const MsgInfo& rhs) const { return rhs.name_ < name_; }

       private:
        static constexpr std::size_t N = 30;
        std::array<float, N> intervals_;
        std::array<std::size_t, N> sizes_;
        std::size_t ix_ = 0;
    };

    using MsgInfoMap = std::map<std::string, MsgInfo>;
    MsgInfoMap messages_;

    MsgInfoMap::iterator selectedEntry_;
    std::string selectedName_;
    MsgInfoMap::iterator displayedEntry_;
    float listWidth_ = 0.0f;

    struct MsgConf
    {
        MsgConf() = delete;
        MsgConf(const std::string& msgName);
        MsgConf(const UBLOXCFG_MSGRATE_t& msgRate);
        std::string msgName_;
        std::string group_;
        std::size_t groupId_ = 0;
        std::string menuNameEnabled_;
        std::string menuNameDisabled_;
        std::unique_ptr<ParserMsg> cmdEnable_;
        std::unique_ptr<ParserMsg> cmdDisable_;
        std::unique_ptr<ParserMsg> cmdPoll_;
        inline bool operator<(const MsgConf& rhs) const { return rhs.msgName_ < msgName_; }
    };

    static std::map<std::string, MsgConf> msgConfs_;
    void _InitMsgConfs();
};

/* ****************************************************************************************************************** */
}  // namespace ffgui
#endif  // __GUI_WIN_DATA_MESSAGES_HPP__

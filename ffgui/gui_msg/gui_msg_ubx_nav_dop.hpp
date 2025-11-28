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

#ifndef __GUI_MSG_UBX_NAV_DOP_HPP__
#define __GUI_MSG_UBX_NAV_DOP_HPP__

//
#include "ffgui_inc.hpp"
//
#include "gui_msg.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

class GuiMsgUbxNavDop : public GuiMsg
{
   public:
    GuiMsgUbxNavDop(const std::string& viewName, const InputPtr& input);

    void Update(const DataMsgPtr& msg) final;
    void Update(const Time& now) final;
    void Clear() final;

    void DrawToolbar() final;
    void DrawMsg() final;


private:
    struct Dop
    {
        float val_ = 0.0f;
        std::string str_;
        float frac_ = 0.0f;
    };

    bool valid_ = false;
    Dop gDop_;
    Dop pDop_;
    Dop tDop_;
    Dop vDop_;
    Dop hDop_;
    Dop nDop_;
    Dop eDop_;

    static constexpr float MAX_DOP = 100.0f;
    static constexpr float LOG_SCALE = 0.1f;
    static constexpr float MAX_LOG = std::log10(MAX_DOP * (1.0 / LOG_SCALE));
#if 0

        void Update(const std::shared_ptr<ParserMsg> &msg) final;
        bool Render(const std::shared_ptr<ParserMsg> &msg, const Vec2f &sizeAvail) final;

    private:

        std::string _gDopStr;
        float       _gDopFrac;
        std::string _pDopStr;
        float       _pDopFrac;
        std::string _tDopStr;
        float       _tDopFrac;
        std::string _vDopStr;
        float       _vDopFrac;
        std::string _hDopStr;
        float       _hDopFrac;
        std::string _nDopStr;
        float       _nDopFrac;
        std::string _eDopStr;
        float       _eDopFrac;

#endif
};

/* ****************************************************************************************************************** */
}  // namespace ffgui
#endif  // __GUI_MSG_UBX_NAV_DOP_HPP__

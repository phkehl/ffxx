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
#include <fpsdk_common/parser/ubx.hpp>
using namespace fpsdk::common::parser::ubx;
#include <ffxx/ublox.hpp>
using namespace ffxx;
#include <ubloxcfg/ubloxcfg.h>
//
#include "gui_msg_ubx_cfg_valxxx.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

using namespace fpsdk::common::parser::ubx;

static HistoryEntries dummyEntries;  // we're not using the filter here

GuiMsgUbxCfgValxxx::GuiMsgUbxCfgValxxx(const std::string& viewName, const InputPtr& input) /* clang-format off */ :
    GuiMsg(viewName, input),
    log_   { viewName, dummyEntries }  // clang-format on
{
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxCfgValxxx::Update(const DataMsgPtr& msg)
{
    const std::size_t size = msg->msg_.Size();
    const uint8_t* data = msg->msg_.Data();
    const uint8_t msgId = UbxMsgId(data);

    log_.AddNotice("%s", msg->msg_.info_.c_str());

    // UBX-CFG-VALGET response
    if ((msgId == UBX_CFG_VALGET_MSGID) && (UBX_CFG_VALGET_VERSION(data) == UBX_CFG_VALGET_V1_VERSION) && (size >= UBX_CFG_VALGET_V1_MIN_SIZE)) {
        UBX_CFG_VALGET_V1_GROUP0 header;
        std::memcpy(&header, &data[UBX_HEAD_SIZE], sizeof(header));
        std::array<UBLOXCFG_KEYVAL_t, UBX_CFG_VALGET_V1_MAX_KV> kvs;
        int nKv = 0;
        if (!ubloxcfg_parseData(&data[UBX_HEAD_SIZE + sizeof(header)], size - UBX_FRAME_SIZE - sizeof(header), kvs.data(), kvs.size(), &nKv)) {
            return;
        }

        log_.AddInfo("%4d + %4d = %4d items", (int)header.position, nKv, (int)header.position + nKv);

        for (int ix = 0; ix < nKv; ix++) {
            const UbloxCfgDb::Item item(kvs[ix].id);
            char value[200];
            if (!ubloxcfg_stringifyValue(value, sizeof(value), item.type_, item.def_, &kvs[ix].val)) {
                value[0] = '?';
                value[1] = '\0';
            }
            log_.AddInfo("%4d + %4d = %4d = 0x%08" PRIx32 " %-35s %-2s %s", (int)header.position, ix,
                (int)header.position + ix, item.id_, item.name_.c_str(), item.typeStr_.c_str(), value);
        }
    }

    // UBX-CFG-VALSET
    else if ((msgId == UBX_CFG_VALSET_MSGID) && (size >= UBX_CFG_VALSET_V0_MIN_SIZE)) {
        UBX_CFG_VALSET_V1_GROUP0 header;
        std::memcpy(&header, &data[UBX_HEAD_SIZE], sizeof(header));
        std::array<UBLOXCFG_KEYVAL_t, UBX_CFG_VALGET_V1_MAX_KV> kvs;
        int nKv = 0;
        if (!ubloxcfg_parseData(&data[UBX_HEAD_SIZE + sizeof(header)], size - UBX_FRAME_SIZE - sizeof(header), kvs.data(), kvs.size(), &nKv)) {
            return;
        }

        for (int ix = 0; ix < nKv; ix++) {
            const UbloxCfgDb::Item item(kvs[ix].id);
            char value[200];
            if (!ubloxcfg_stringifyValue(value, sizeof(value), item.type_, item.def_, &kvs[ix].val)) {
                value[0] = '?';
                value[1] = '\0';
            }
            log_.AddInfo("%2d / %2d : 0x%08" PRIx32 " %-35s %-2s %s", ix + 1, nKv, item.id_, item.name_.c_str(), item.typeStr_.c_str(), value);
        }
    }

    // UBX-CFG-VALDEL
    else if ((msgId == UBX_CFG_VALDEL_MSGID) && (size >= UBX_CFG_VALDEL_V1_MIN_SIZE)) {
        UBX_CFG_VALDEL_V1_GROUP0 header;
        std::memcpy(&header, &data[UBX_HEAD_SIZE], sizeof(header));
        std::array<uint32_t, UBX_CFG_VALDEL_V1_MAX_K> ks;
        const int nK = ((size - UBX_FRAME_SIZE - sizeof(header)) / sizeof(uint32_t));
        std::memcpy(ks.data(), &data[UBX_HEAD_SIZE + sizeof(header)], nK * sizeof(uint32_t));

        for (int ix = 0; ix < nK; ix++) {
            const UbloxCfgDb::Item item(ks[ix]);
            log_.AddInfo("%2d / %2d : 0x%08" PRIx32 " %s", ix + 1, nK, item.id_, item.name_.c_str());
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxCfgValxxx::Update(const Time& /*now*/)
{
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxCfgValxxx::Clear()
{
    log_.Clear();
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxCfgValxxx::DrawToolbar()
{
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiMsgUbxCfgValxxx::DrawMsg()
{
    log_.DrawWidget();
}

/* ****************************************************************************************************************** */
}  // namespace ffgui

/**
 * \verbatim
 * flipflip's c++ library (ffxx)
 *
 * Copyright (c) Philippe Kehl (flipflip at oinkzwurgl dot org)
 * https://oinkzwurgl.org/projaeggd/ffxx/
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the
 * GNU General Public License as published by the Free Software Foundation, either version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
 * even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program.
 * If not, see <https://www.gnu.org/licenses/>.
 * \endverbatim
 *
 * @file
 * @brief u-blox receiver stuff
 */

/* LIBC/STL */
#include <algorithm>
#include <cstring>
#include <unordered_map>

/* EXTERNAL */
#include <fpsdk_common/logging.hpp>
#include <fpsdk_common/string.hpp>
#include <fpsdk_common/parser.hpp>
#include <fpsdk_common/parser/ubx.hpp>

/* PACKAGE */
#include "ublox.hpp"

namespace ffxx {
/* ****************************************************************************************************************** */

using namespace fpsdk::common::parser;
using namespace fpsdk::common::parser::ubx;
using namespace fpsdk::common::string;

// ---------------------------------------------------------------------------------------------------------------------

UbloxCfgDb::UbloxCfgDb(const std::string& name) /* clang-format off */ :
    name_   { name } // clang-format on
{
}

UbloxCfgDb::~UbloxCfgDb()
{
}

// ---------------------------------------------------------------------------------------------------------------------

void UbloxCfgDb::StartValGetResp()
{
    Clear();
}

std::size_t UbloxCfgDb::AddValGetResp(const fpsdk::common::parser::ParserMsg& msg)
{
    msg.MakeInfo();
    DEBUG("%s AddValGetResp %s", name_.c_str(), msg.info_.c_str());

    if ((msg.name_ != UBX_CFG_VALGET_STRID) || (UBX_CFG_VALGET_VERSION(msg.Data()) != UBX_CFG_VALGET_V1_VERSION) ||
        (msg.Size() < UBX_CFG_VALGET_V1_MIN_SIZE)) {
        return 0;
    }

    UBX_CFG_VALGET_V1_GROUP0 valget;
    std::memcpy(&valget, &msg.data_[UBX_HEAD_SIZE], sizeof(valget));

    const uint8_t* data = &msg.data_[UBX_HEAD_SIZE + sizeof(valget)];
    const std::size_t size = msg.Size() - UBX_FRAME_SIZE - sizeof(valget);
    int nKeyVal = 0;
    std::array<UBLOXCFG_KEYVAL_t, UBX_CFG_VALGET_V1_MAX_KV> kv;
    if (!ubloxcfg_parseData(data, size, kv.data(), kv.size(), &nKeyVal)) {
        return 0;
    }

    DEBUG("%s AddValGetResp nKeyVal=%d", name_.c_str(), nKeyVal);

    for (int ix = 0; ix < nKeyVal; ix++) {
        const uint32_t id = kv[ix].id;
        auto item = std::find_if(items_.begin(), items_.end(), [id](const auto& cand) { return cand.id_ == id; });
        if (item == items_.end()) {
            item = items_.insert(items_.end(), id);
        }
        switch (valget.layer) { // clang-format off
            case UBX_CFG_VALGET_V1_LAYER_DEFAULT: item->SetValue(UBLOXCFG_LAYER_DEFAULT, kv[ix].val); break;
            case UBX_CFG_VALGET_V1_LAYER_FLASH:   item->SetValue(UBLOXCFG_LAYER_FLASH,   kv[ix].val); break;
            case UBX_CFG_VALGET_V1_LAYER_BBR:     item->SetValue(UBLOXCFG_LAYER_BBR,     kv[ix].val); break;
            case UBX_CFG_VALGET_V1_LAYER_RAM:     item->SetValue(UBLOXCFG_LAYER_RAM,     kv[ix].val); break;
        } // clang-format on
    }
    return nKeyVal;
}

void UbloxCfgDb::EndValGetResp()
{
    // LUT: Group ID -> group name
    std::unordered_map<uint32_t, std::string> groupNames;
    for (const auto& item : items_) {
        if (item.def_) {
            groupNames[item.groupId_] = item.group_;
        }
    }

    // Work out better names for unknown items
    for (auto& item : items_)
    {
        if (!item.def_) {
            const auto groupName = groupNames.find(item.groupId_);
            if (groupName != groupNames.end()) {
                item.group_ = groupName->second;
            } else {
                item.group_ = Sprintf("CFG-%03" PRIx32, item.groupId_ >> 16);
            }
            item.name_ = Sprintf("%s-%04" PRIx32, item.group_.c_str(), item.id_ & 0xffff);
        }
    }

    // Sort
    std::sort(items_.begin(), items_.end(), [](const auto& a, const auto& b) {
        return (a.id_ & 0x0fffffff) < (b.id_ & 0x0fffffff);  // ignore size
    });

    std::swap(items_, items_);
}

// ---------------------------------------------------------------------------------------------------------------------

void UbloxCfgDb::Clear()
{
    items_.clear();
}

UbloxCfgDb::Items& UbloxCfgDb::GetItems()
{
    return items_;
}

// ---------------------------------------------------------------------------------------------------------------------

UbloxCfgDb::Item::Item(const uint32_t id) /* clang-format off */ :
    id_   { id } // clang-format on
{
    const UBLOXCFG_ITEM_t *def = ubloxcfg_getItemById(id);
    if (def) {
        name_ = def->name;
        group_ = def->group;
        groupId_ = UBLOXCFG_ID2GROUP(id_);
        size_ = def->size;
        type_ = def->type;
        typeStr_ = ubloxcfg_typeStr(def->type);
        def_ = def;
        title_ = def->title;
        unit_ = (def->unit ? def->unit : "-");
        scale_ = (def->scale ? def->scale : "-");
    } else {
        name_ = Sprintf("0x%" PRIx32, id_);                // But see...
        group_ = Sprintf("%08x", UBLOXCFG_ID2GROUP(id_));  // ...EndValGetResp()
        groupId_ = UBLOXCFG_ID2GROUP(id_);
        size_ = UBLOXCFG_ID2SIZE(id_);
        switch (size_) { // clang-format off
            case UBLOXCFG_SIZE_BIT:   type_ = UBLOXCFG_TYPE_L;  break;
            case UBLOXCFG_SIZE_ONE:   type_ = UBLOXCFG_TYPE_X1; break;
            case UBLOXCFG_SIZE_TWO:   type_ = UBLOXCFG_TYPE_X2; break;
            case UBLOXCFG_SIZE_FOUR:  type_ = UBLOXCFG_TYPE_X4; break;
            case UBLOXCFG_SIZE_EIGHT: type_ = UBLOXCFG_TYPE_X8; break;
        } // clang-format on
        typeStr_ = ubloxcfg_typeStr(type_);
        title_ = "Unknown (undocumented) configuration item";
        unit_ = "-";
        scale_ = "-";
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void UbloxCfgDb::Item::SetValue(const UBLOXCFG_LAYER_t layer, const UBLOXCFG_VALUE_t& val)
{
    Value* pVal = nullptr;
    switch (layer) { // clang-format off
        case UBLOXCFG_LAYER_DEFAULT: pVal = &valDefault_; break;
        case UBLOXCFG_LAYER_FLASH:   pVal = &valFlash_;   break;
        case UBLOXCFG_LAYER_BBR:     pVal = &valBbr_;     break;
        case UBLOXCFG_LAYER_RAM:     pVal = &valRam_;     break;
    } // clang-format on
    if (pVal)
    {
        pVal->valid_ = true;
        pVal->val_  = val;
        char str[100];
        if (ubloxcfg_stringifyValue(str, sizeof(str), type_, def_, &val)) {
            pVal->str_ = str;
        }
    }
}

/* ****************************************************************************************************************** */

std::optional<fpsdk::common::parser::ParserMsg> MakeUbxCfgValgetAll(const UBLOXCFG_LAYER_t layer, const std::size_t offs)
{
    UBX_CFG_VALGET_V0_GROUP0 valget;
    valget.version = UBX_CFG_VALGET_V0_VERSION;  // Poll
    valget.layer = 0;
    switch (layer) {  // clang-format off
        case UBLOXCFG_LAYER_DEFAULT: valget.layer = UBX_CFG_VALGET_V1_LAYER_DEFAULT; break;
        case UBLOXCFG_LAYER_FLASH:   valget.layer = UBX_CFG_VALGET_V1_LAYER_FLASH;   break;
        case UBLOXCFG_LAYER_BBR:     valget.layer = UBX_CFG_VALGET_V1_LAYER_BBR;     break;
        case UBLOXCFG_LAYER_RAM:     valget.layer = UBX_CFG_VALGET_V1_LAYER_RAM;     break;
    }  // clang-format on
    valget.position = std::clamp<uint16_t>(offs, 0, 5000);
    const uint32_t keys = UBX_CFG_VALGET_V1_ALL_WILDCARD;

    std::vector<uint8_t> payload(sizeof(valget) + sizeof(keys));
    std::memcpy(&payload[0], &valget, sizeof(valget));
    std::memcpy(&payload[sizeof(valget)], &keys, sizeof(keys));

    auto msg = MakeUbxParserMsg(UBX_CFG_CLSID, UBX_CFG_VALGET_MSGID, payload);
    // DEBUG("MakeUbxCfgValgetAll %s", msg.info_.c_str());
    if (msg) {
        (*msg).seq_ = (offs / UBX_CFG_VALGET_V1_MAX_KV) + 1;  // probably...
    }
    return msg;
}

// ---------------------------------------------------------------------------------------------------------------------

std::vector<ParserMsg> MakeUbxCfgValdel(const std::vector<UBLOXCFG_LAYER_t>& layers, const std::vector<uint32_t>& ids)
{
    std::vector<ParserMsg> msgs;

    for (std::size_t ix = 0; ix < ids.size(); ix += UBX_CFG_VALDEL_V1_MAX_K) {
        UBX_CFG_VALDEL_V1_GROUP0 valdel;
        valdel.version = UBX_CFG_VALDEL_V1_VERSION;
        valdel.reserved = UBX_CFG_VALDEL_V1_RESERVED;
        valdel.layers = 0;
        for (auto layer : layers) {
            switch (layer) {  // clang-format off
                case UBLOXCFG_LAYER_FLASH: valdel.layers |= UBX_CFG_VALDEL_V1_LAYER_FLASH; break;
                case UBLOXCFG_LAYER_BBR:   valdel.layers |= UBX_CFG_VALDEL_V1_LAYER_BBR;   break;
                case UBLOXCFG_LAYER_RAM:
                case UBLOXCFG_LAYER_DEFAULT: break;
            }  // clang-format on
        }
        if (ids.size() <= UBX_CFG_VALDEL_V1_MAX_K) {
            valdel.transaction = UBX_CFG_VALDEL_V1_TRANSACTION_NONE;
        } else if (ix == 0) {
            valdel.transaction = UBX_CFG_VALDEL_V1_TRANSACTION_BEGIN;
        } else if ((ids.size() - ix) > UBX_CFG_VALDEL_V1_MAX_K) {
            valdel.transaction = UBX_CFG_VALDEL_V1_TRANSACTION_CONTINUE;
        } else {
            valdel.transaction = UBX_CFG_VALDEL_V1_TRANSACTION_END;
        }
        const std::size_t nKeys = std::min(ids.size() - ix, UBX_CFG_VALDEL_V1_MAX_K);
        const std::size_t sKeys = nKeys * sizeof(uint32_t);

        std::vector<uint8_t> payload(sizeof(valdel) + sKeys);
        std::memcpy(&payload[0], &valdel, sizeof(valdel));
        std::memcpy(&payload[sizeof(valdel)], &ids[ix], sKeys);

        auto msg = MakeUbxParserMsg(UBX_CFG_CLSID, UBX_CFG_VALDEL_MSGID, payload);
        if (msg) {
            (*msg).seq_ = (ix / UBX_CFG_VALDEL_V1_MAX_K) + 1;
            msgs.push_back(std::move(msg.value()));
        } else {
            return {};
        }
    }

    return msgs;
}

// ---------------------------------------------------------------------------------------------------------------------

// It seems that when using transactions the last message (with UBX-CFG-VALSET.transaction = 3, i.e. end/commit) must
// not contain any key-value pairs or those key-value pairs in the last message are ignored and not applied.
#define NEED_EMPTY_TRANSACTION_END


std::vector<ParserMsg> MakeUbxCfgValset(const std::vector<UBLOXCFG_LAYER_t>& layers, const std::vector<UBLOXCFG_KEYVAL_t>& kvs)
{
    std::vector<ParserMsg> msgs;

    for (std::size_t ix = 0; ix < kvs.size(); ix += UBX_CFG_VALSET_V1_MAX_KV) {
        UBX_CFG_VALSET_V1_GROUP0 valset;
        valset.version = UBX_CFG_VALSET_V1_VERSION;
        valset.reserved = UBX_CFG_VALSET_V1_RESERVED;
        valset.layers = 0;
        for (auto layer : layers) {
            switch (layer) {  // clang-format off
                case UBLOXCFG_LAYER_FLASH: valset.layers |= UBX_CFG_VALSET_V1_LAYER_FLASH; break;
                case UBLOXCFG_LAYER_BBR:   valset.layers |= UBX_CFG_VALSET_V1_LAYER_BBR;   break;
                case UBLOXCFG_LAYER_RAM:   valset.layers |= UBX_CFG_VALSET_V1_LAYER_RAM;   break;
                case UBLOXCFG_LAYER_DEFAULT: break;
            }  // clang-format on
        }
        if (kvs.size() <= UBX_CFG_VALSET_V1_MAX_KV) {
            valset.transaction = UBX_CFG_VALSET_V1_TRANSACTION_NONE;
        } else if (ix == 0) {
            valset.transaction = UBX_CFG_VALSET_V1_TRANSACTION_BEGIN;
        } else if ((kvs.size() - ix) > UBX_CFG_VALSET_V1_MAX_KV) {
            valset.transaction = UBX_CFG_VALSET_V1_TRANSACTION_CONTINUE;
        } else {
#ifdef NEED_EMPTY_TRANSACTION_END
            valset.transaction = UBX_CFG_VALSET_V1_TRANSACTION_CONTINUE;
#else
            valset.transaction = UBX_CFG_VALSET_V1_TRANSACTION_END;
#endif
        }
        const std::size_t nKv = std::min(kvs.size() - ix, UBX_CFG_VALSET_V1_MAX_KV);
        const std::size_t sKvMax = nKv * (sizeof(uint32_t) + sizeof(UBLOXCFG_VALUE_t));

        std::vector<uint8_t> payload(sizeof(valset) + sKvMax);
        std::memcpy(&payload[0], &valset, sizeof(valset));
        int sKv = 0;
        ubloxcfg_makeData(&payload[sizeof(valset)], sKvMax, &kvs[ix], nKv, &sKv);
        payload.resize(sizeof(valset) + sKv);

        auto msg = MakeUbxParserMsg(UBX_CFG_CLSID, UBX_CFG_VALSET_MSGID, payload);
        if (msg) {
            (*msg).seq_ = msgs.size() + 1;
            msgs.push_back(std::move(msg.value()));
        } else {
            return {};
        }
    }

#ifdef NEED_EMPTY_TRANSACTION_END
    if (kvs.size() > UBX_CFG_VALSET_V1_MAX_KV) {
        UBX_CFG_VALSET_V1_GROUP0 valset;
        std::memcpy(&valset, &msgs.back().Data()[UBX_HEAD_SIZE], sizeof(valset));
        valset.transaction = UBX_CFG_VALSET_V1_TRANSACTION_END;
        const std::vector<uint8_t> payload { (const uint8_t*)&valset, (const uint8_t*)&valset + sizeof(valset) };
        auto msg = MakeUbxParserMsg(UBX_CFG_CLSID, UBX_CFG_VALSET_MSGID, payload);
        if (msg) {
            (*msg).seq_ = msgs.size() + 1;
            msgs.push_back(std::move(msg.value()));
        } else {
            return {};
        }
    }
#endif

    return msgs;
}

// ---------------------------------------------------------------------------------------------------------------------

std::optional<fpsdk::common::parser::ParserMsg> MakeUbxParserMsg(const uint8_t clsId, const uint8_t msgId, const std::vector<uint8_t>& payload, const bool makeInfo)
{
    std::vector<uint8_t> raw;
    if (!UbxMakeMessage(raw, clsId, msgId, payload)) {
        return std::nullopt;
    }

    Parser parser;
    ParserMsg msg;
    if (!parser.Add(raw) || !parser.Process(msg)) {
        return std::nullopt;
    }
    if (makeInfo) {
        msg.MakeInfo();
    }

    return msg;
}


/* ****************************************************************************************************************** */
}  // namespace ffxx

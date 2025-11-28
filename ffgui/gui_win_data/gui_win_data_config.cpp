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
#include <fstream>
#include <functional>
//
#include <fpsdk_common/parser/ubx.hpp>
//
#include "gui_notify.hpp"
#include "gui_win_data_config.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

using namespace fpsdk::common::parser::ubx;

// ---------------------------------------------------------------------------------------------------------------------

static constexpr const char* TB_DATABASE = "Database";
static constexpr const char* TB_LOG = "Log";

static HistoryEntries dummyEntries;  // we're not using the filter here

GuiWinDataConfig::GuiWinDataConfig(const std::string& name, const InputPtr& input) /* clang-format off */ :
    GuiWinData(name, { 160, 40, 40, 40 }, ImGuiWindowFlags_None, input),
    tabbarMain_   { WinName() + ".Main" },
    tabbarEdit_   { WinName() + ".Edit", ImGuiTabBarFlags_TabListPopupButton | ImGuiTabBarFlags_FittingPolicyScroll },
    log_          { WinName(), dummyEntries, false },
    filter_       { WinName(), cfg_.history },
    db_           { WinName() }  // clang-format on
{
    DEBUG("GuiWinDataConfig(%s)", WinName().c_str());
    GuiGlobal::LoadObj(WinName() + ".GuiWinDataConfig", cfg_);
    latestEpochMaxAge_ = {};

    ClearData();
    _SetState(State::IDLE);
}

GuiWinDataConfig::~GuiWinDataConfig()
{
    DEBUG("~GuiWinDataConfig(%s)", WinName().c_str());

    std::transform(itemsEdit_.begin(), itemsEdit_.end(), std::inserter(cfg_.selectedItems, cfg_.selectedItems.begin()),
        [](const auto* item) { return item->db_.name_; });

    GuiGlobal::SaveObj(WinName() + ".GuiWinDataConfig", cfg_);
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataConfig::_ProcessData(const DataPtr& data)
{
    if (data->type_ == DataType::MSG) {
        const auto& msg = DataPtrToDataMsg(data);
        switch (state_) {
            case State::IDLE:
                break;
            case State::POLL:
                _PollData(msg);
                break;
            case State::SAVE:
                _SaveData(msg);
                break;
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataConfig::_Loop(const Time& now)
{
    switch (state_) {
        case State::IDLE:
            break;
        case State::POLL:
            _PollLoop(now);
            break;
        case State::SAVE:
            _SaveLoop(now);
            break;
    }

    if (itemsDirty_) {
        _Sync();
        itemsDirty_ = false;
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataConfig::_ClearData()
{
    db_.Clear();
    itemsAll_.clear();
    itemsDirty_ = true;
    log_.Clear();
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataConfig::_DrawToolbar()
{
    Gui::VerticalSeparator();

    // ----- list display options -----

    ImGui::BeginDisabled(cfg_.showSelected);
    if (Gui::ToggleButton(ICON_FK_DOT_CIRCLE_O "##showHidden", ICON_FK_CIRCLE_O "##showHidden", &cfg_.showHidden,
            "Showing hidden (undocumented) items", "Not showing hidden (undocumented) items", GUI_VAR.iconSize)) {
        itemsDirty_ = true;
    }
    ImGui::EndDisabled();

    ImGui::SameLine();

    if (Gui::ToggleButton(ICON_FK_CHECK_CIRCLE_O "##showSelected", ICON_FK_CIRCLE "##showSelected", &cfg_.showSelected,
            "Showing only selected items", "Showing all items", GUI_VAR.iconSize)) {
        itemsDirty_ = true;
    }

    ImGui::SameLine();

    if (ImGui::Button(viewIx_ == 0 ? ICON_FK_BOOKMARK_O "##View" : ICON_FK_BOOKMARK "##View", GUI_VAR.iconSize)) {
        ImGui::OpenPopup("View");
    }
    Gui::ItemTooltip("Show items for common configuration tasks");
    if (ImGui::BeginPopup("View")) {
        Gui::TextTitle("Show items for:");
        ImGui::Separator();
        for (std::size_t ix = 0; ix < VIEWS.size(); ix++) {
            if (ImGui::Selectable(VIEWS[ix].title_, ix == viewIx_)) {
                _SetView(ix);
            }
        }
        ImGui::EndPopup();
    }

    ImGui::SameLine();

    const float spaceX = ImGui::GetContentRegionAvail().x;

    if (filter_.DrawWidget(0.25f * spaceX)) {
        itemsDirty_ = true;
    }

    Gui::VerticalSeparator();

    // ----- actions -----

    const bool receiverReady = (input_->receiver_->GetState() == StreamState::CONNECTED);
    const bool idle = (state_ == State::IDLE);
    ImGui::BeginDisabled(!receiverReady || !idle);

    // Refresh items
    if (ImGui::Button(ICON_FK_REFRESH "##RefreshItems", GUI_VAR.iconSize)) {
        _PollStart();
    }
    Gui::ItemTooltip("Load configuration data from receiver");

    ImGui::SameLine();

    ImGui::BeginDisabled(itemsPending_ == 0);
    if (ImGui::Button(ICON_FK_THUMBS_UP, GUI_VAR.iconSize)) {
        _SaveStart();
    }
    Gui::ItemTooltip("Save configuration changes");
    ImGui::EndDisabled();

    ImGui::SameLine();

    ImGui::EndDisabled();  // !receiverReady || !idle

    const float progressBarWidth = ImGui::GetContentRegionAvail().x - GUI_VAR.iconSize.x - GUI_VAR.imguiStyle->ItemSpacing.x;

    // Status (progress bar, ...)
    switch (state_) {
        case State::IDLE: {
            char str[100];
            std::snprintf(str, sizeof(str), "Config changes: %" PRIuMAX, itemsPending_);
            ImGui::ProgressBar(-1.0f, { progressBarWidth, 0 }, str);
            break;
        }
        case State::POLL: {
            static constexpr std::size_t ITEMS_PER_LAYER = 1500;  // ca.
            static constexpr std::size_t TOTAL_ITEMS = NumOf(UbloxCfgDb::LAYERS) * ITEMS_PER_LAYER;
            const std::size_t numItems = (poll_.layerIx_ * ITEMS_PER_LAYER) + poll_.offs_;
            const float progress = (float)numItems / (float)TOTAL_ITEMS;
            char str[100];
            std::snprintf(str, sizeof(str), "Poll %s (total %.0f%%)",
                ubloxcfg_layerName(UbloxCfgDb::LAYERS[poll_.layerIx_]), progress * 1e2f);
            ImGui::ProgressBar(progress, { progressBarWidth, 0 }, str);
            break;
        }
        case State::SAVE: {
            const float progress = (float)save_.ix_ / (float)save_.msgs_.size();
            char str[100];
            std::snprintf(str, sizeof(str), "Save configuration (total %.0f%%)", progress * 1e2f);
            ImGui::ProgressBar(progress, { progressBarWidth, 0 }, str);
            break;
        }
    }

    ImGui::SameLine();
    Gui::HelpButton(HELP_TEXT);
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataConfig::_DrawContent()
{
    ImGui::Separator();

    bool doDatabase = false;
    if (tabbarMain_.Begin()) {
        if (tabbarMain_.Item(TB_DATABASE)) { doDatabase = true; }
        tabbarMain_.Item(TB_LOG, [this]() { log_.DrawWidget(); });
        tabbarMain_.End();
    }


    if (doDatabase) {

        if (itemsEdit_.empty()) {
            _DrawItems();
        } else {
            auto avail = ImGui::GetContentRegionAvail();

            const ImVec2 editSize = { avail.x,
                ((GUI_VAR.lineHeight + GUI_VAR.imguiStyle->FramePadding.y) * 5.0f) +
                (GUI_VAR.imguiStyle->ItemSpacing.y * 2.0f * 3.5f) };
            const ImVec2 listSize = { avail.x, std::max(editSize.y, avail.y - editSize.y - GUI_VAR.imguiStyle->ItemSpacing.y) };
            if (ImGui::BeginChild("List", listSize)) {
                _DrawItems();
            }
            ImGui::EndChild();
            if (ImGui::BeginChild("Edit", editSize)) {
                _DrawEditorTabs();
            }
            ImGui::EndChild();
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataConfig::_DrawItems()
{
    ImGui::BeginDisabled(state_ != State::IDLE);

    const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_NoSavedSettings |
                                  ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY;

    if (/*drawChild &&*/ ImGui::BeginTable("List", 10, flags, ImGui::GetContentRegionAvail())) {
        ImGui::TableSetupScrollFreeze(1, 1); // Make top row always visible
        const float wx = GUI_VAR.charSizeX;
        ImGui::TableSetupColumn("Item", ImGuiTableColumnFlags_WidthFixed, wx * (float)COL_WIDTH_ITEM);
        ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, wx * (float)COL_WIDTH_ID);
        ImGui::TableSetupColumn("T", ImGuiTableColumnFlags_WidthFixed, wx * (float)COL_WIDTH_TYPE);
        ImGui::TableSetupColumn("Unit", ImGuiTableColumnFlags_WidthFixed, wx * (float)COL_WIDTH_UNIT);
        ImGui::TableSetupColumn("Scale", ImGuiTableColumnFlags_WidthFixed, wx * (float)COL_WIDTH_SCALE);
        ImGui::TableSetupColumn(Item::LAYER_NAMES[Item::DEF], ImGuiTableColumnFlags_WidthFixed, wx * (float)COL_WIDTH_VALUE);
        ImGui::TableSetupColumn(Item::LAYER_NAMES[Item::FLASH], ImGuiTableColumnFlags_WidthFixed, wx * (float)COL_WIDTH_VALUE);
        ImGui::TableSetupColumn(Item::LAYER_NAMES[Item::BBR], ImGuiTableColumnFlags_WidthFixed, wx * (float)COL_WIDTH_VALUE);
        ImGui::TableSetupColumn(Item::LAYER_NAMES[Item::RAM], ImGuiTableColumnFlags_WidthFixed, wx * (float)COL_WIDTH_VALUE);
        ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthFixed, wx * (float)COL_WIDTH_DESC);

        // ImGui::TableHeadersRow();
        ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
        const int nCol = ImGui::TableGetColumnCount();
        for (int colIx = 0; colIx < nCol; colIx++) {
            if (!ImGui::TableSetColumnIndex(colIx)) {
                continue;
            }
            ImGui::TableHeader(ImGui::TableGetColumnName(colIx));
            const Item::ValueIx valIx = (Item::ValueIx)(colIx - 5);
            if ((valIx == Item::DEF) && ImGui::BeginPopupContextItem("HeaderDef")) {
                _DrawHeaderPopup(valIx);
                ImGui::EndPopup();
            }
            else if ((valIx == Item::FLASH) && ImGui::BeginPopupContextItem("HeaderFlash")) {
                _DrawHeaderPopup(valIx);
                ImGui::EndPopup();
            }
            else if ((valIx == Item::BBR) && ImGui::BeginPopupContextItem("HeaderBbr")) {
                _DrawHeaderPopup(valIx);
                ImGui::EndPopup();
            }
            else if ((valIx == Item::RAM) && ImGui::BeginPopupContextItem("HeaderRam")) {
                _DrawHeaderPopup(valIx);
                ImGui::EndPopup();
            }
        }

        ImGuiListClipper clipper;
        clipper.Begin(itemsDisp_.size());
        const bool haveFilter = filter_.IsActive();
        const bool highlight = haveFilter && filter_.IsHighlight();

        while (clipper.Step()) {

            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++)
            {
                if (!itemsDisp_[row]) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Dummy(GUI_VAR.charSize);
                    continue;
                }


                auto& item = *itemsDisp_[row];

                ImGui::TableNextRow();
                ImGui::PushID(row);

                const bool dim = (highlight && !item.filterMatch_);
                if (dim) { ImGui::PushStyleVar(ImGuiStyleVar_Alpha, GUI_VAR.alphaDimVal); }

                ImGui::TableNextColumn();
                // ImGui::SameLine(0, GUI_VAR.charSizeX * 2.0f);
                const bool selected = item.selected_;
                if (ImGui::Selectable(item.db_.name_.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns)) {
                    auto& io = ImGui::GetIO();
                    if (io.KeyCtrl) {
                        item.selected_ = !selected;
                    } else {
                        for (auto& i : itemsAll_) {
                            i.selected_ = false;
                        }
                        item.selected_ = !selected;
                    }
                    itemsDirty_ = true;
                    tabbarEdit_.Switch(item.db_.name_);
                }
                if (ImGui::BeginPopupContextItem("ItemRow")) {
                    _DrawItemPopup(item);
                    ImGui::EndPopup();
                }

#ifndef NDEBUG
                ImGui::SameLine();
                Gui::TextDim("(?)");
                Gui::ItemTooltip(item.debug_);
#endif

                ImGui::TableNextColumn();
                ImGui::Text("0x%08" PRIx32, item.db_.id_);

                ImGui::TableNextColumn();
                ImGui::TextUnformatted(item.db_.typeStr_.c_str());

                ImGui::TableNextColumn();
                ImGui::TextUnformatted(item.db_.unit_.c_str());

                ImGui::TableNextColumn();
                ImGui::TextUnformatted(item.db_.scale_.c_str());

                for (int ix = Item::DEF; ix <= Item::RAM; ix++) {
                    const auto& value = item.editValues_[ix];
                    const bool edited = item.valuesEdited_[ix];
                    const bool nonDefault = (edited ? item.editNonDefault_[ix] : item.currNonDefault_[ix]);

                    ImGui::TableNextColumn();
                    if (nonDefault) { ImGui::PushStyleColor(ImGuiCol_Text, C_TEXT_HIGHLIGHT_FG()); }
                    if (edited) { ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, C_TEXT_HIGHLIGHT_BG()); }

                    if (value.str_.size() > COL_WIDTH_VALUE) {
                        ImGui::Text("%.*s…", (int)COL_WIDTH_VALUE -1, value.str_.c_str());
                        Gui::ItemTooltip(value.str_);
                    } else {
                        ImGui::TextUnformatted(value.str_.c_str());
                    }
                    if (nonDefault) { ImGui::PopStyleColor(); }
                }

                ImGui::TableNextColumn();
                if (item.db_.title_.size() > COL_WIDTH_DESC) {
                    ImGui::Text("%.*s…", (int)COL_WIDTH_DESC - 1, item.db_.title_.c_str());
                    Gui::ItemTooltip(item.db_.title_);
                } else {
                    ImGui::TextUnformatted(item.db_.title_.c_str());
                }

                if (dim) { ImGui::PopStyleVar(); }

                ImGui::PopID();
            }
        }

        ImGui::EndTable();
    }

    ImGui::EndDisabled();
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataConfig::_DrawEditorTabs()
{
    if (!tabbarEdit_.Begin()) {
        return;
    }
    Item* editItem = nullptr;
    for (auto item : itemsEdit_) {
        bool close = false;
        if (tabbarEdit_.Item(item->db_.name_, &close,
                item->numEdited_ > 0 ? ImGuiTabItemFlags_UnsavedDocument : ImGuiTabItemFlags_None)) {
            editItem = item;
        }
        if (close) {
            item->selected_ = false;
            itemsDirty_ = true;
        }
    }
    tabbarEdit_.End();

    if (editItem) {
        _DrawItemEditor(*editItem);
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataConfig::_DrawItemEditor(Item& item)
{
    const float indent1 = GUI_VAR.charSizeX * 10.0f;
    const float indent2 = GUI_VAR.charSizeX * ((float)COL_WIDTH_VALUE + 2.0f) + indent1;
    const float indent3 = GUI_VAR.charSizeX * ((float)COL_WIDTH_VALUE + 6.0f) + indent2;

    for (int ix = Item::DEF; ix <= Item::RAM; ix++) {
        const auto& currValue = item.currValues_[ix];
        auto& editValue = item.editValues_[ix];
        const bool edited = item.valuesEdited_[ix];
        const bool nonDefault = (edited ? item.editNonDefault_[ix] : item.currNonDefault_[ix]);
        ImGui::PushID(std::addressof(editValue));

        // Column 1: Layer name
        ImGui::AlignTextToFramePadding();
        Gui::TextTitle(Item::LAYER_NAMES[ix]);

        // Column 2: Current value
        ImGui::SameLine(indent1);
        if (currValue.valid_) {
            if (currValue.str_.size() > COL_WIDTH_VALUE) {
                ImGui::Text("%.*s…", (int)COL_WIDTH_VALUE - 1, currValue.str_.c_str());
                Gui::ItemTooltip(currValue.str_);
            } else {
                ImGui::TextUnformatted(currValue.str_.c_str());
            }
        } else {
            ImGui::TextUnformatted("(value not set)");
        }

        // Column 3:
        ImGui::SameLine(indent2);
        // - Layer default: item info
        if (ix == Item::DEF) {
            Gui::TextTitle(item.db_.name_);
            ImGui::SameLine();
            ImGui::Text("(0x%08" PRIx32 " %s, %s, %s) %s", item.db_.id_, item.db_.typeStr_.c_str(),
                item.db_.scale_.c_str(), item.db_.unit_.c_str(), item.db_.title_.c_str());
        }
        // - Other layers: Edited value
        else {
            ImGui::TextUnformatted(ICON_FK_LONG_ARROW_RIGHT);
            ImGui::SameLine();

            if (nonDefault) { ImGui::PushStyleColor(ImGuiCol_Text, C_TEXT_HIGHLIGHT_FG()); }
            ImGui::Text(edited ? "*" : " ");
            ImGui::SameLine();
            if (editValue.valid_) {
                if (editValue.str_.size() > COL_WIDTH_VALUE) {
                    ImGui::Text("%.*s…", (int)COL_WIDTH_VALUE - 1, editValue.str_.c_str());
                    Gui::ItemTooltip(editValue.str_);
                } else {
                    ImGui::TextUnformatted(editValue.str_.c_str());
                }
            } else {
                ImGui::TextUnformatted("(value not set)");
            }
            if (nonDefault) { ImGui::PopStyleColor(); }
        }

        // Column 4: value editor
        if (ix != Item::DEF) {
            ImGui::SameLine(indent3);

            ImGui::BeginDisabled(!nonDefault);

            if (ImGui::Button(ICON_FK_TIMES, GUI_VAR.iconSize)) {
                editValue = (ix == Item::RAM ? item.db_.valDefault_ : UbloxCfgDb::Value());
                itemsDirty_ = true;

            }
            Gui::ItemTooltip(ix == Item::RAM ? "Revert to default" : "Remove value");
            ImGui::EndDisabled();

            ImGui::SameLine(0.0f, GUI_VAR.imguiStyle->ItemInnerSpacing.x);

            ImGui::BeginDisabled(!edited);
            if (ImGui::Button(ICON_FK_UNDO, GUI_VAR.iconSize)) {
                editValue = currValue;
                itemsDirty_ = true;
            }
            Gui::ItemTooltip("Undo edit (revert to current value)");
            ImGui::EndDisabled();

            ImGui::SameLine();

            if (_DrawValueEditor(editValue, item.db_)) {
                itemsDirty_ = true;
            }
        }

        ImGui::PopID();
        ImGui::Separator();
    }
}

void GuiWinDataConfig::_DrawItemPopup(Item& item)
{
    _DrawItemEditor(item);
    if (ImGui::Selectable("Copy item name")) {
        ImGui::SetClipboardText(item.db_.name_.c_str());
    }
    if (ImGui::Selectable("Copy item ID")) {
        ImGui::SetClipboardText(Sprintf("0x%08" PRIu32, item.db_.id_).c_str());
    }
}

// ---------------------------------------------------------------------------------------------------------------------

bool GuiWinDataConfig::_DrawValueEditor(UbloxCfgDb::Value& dbValue, const UbloxCfgDb::Item& dbItem)
{
    bool changed = false;
    const float smallSpacing = GUI_VAR.imguiStyle->ItemInnerSpacing.x;

    // Value
    switch (dbItem.type_) {
        case UBLOXCFG_TYPE_I1:
        case UBLOXCFG_TYPE_I2:
        case UBLOXCFG_TYPE_I4:
        case UBLOXCFG_TYPE_I8:
        case UBLOXCFG_TYPE_E1:
        case UBLOXCFG_TYPE_E2:
        case UBLOXCFG_TYPE_E4: {
            int64_t val = dbValue.val_.I1;
            int64_t min = INT8_MIN;
            int64_t max = INT8_MAX;
            int width = 5;
            switch (dbItem.type_) {  // clang-format off
                case UBLOXCFG_TYPE_E1:
                case UBLOXCFG_TYPE_I1: break;
                case UBLOXCFG_TYPE_E2:
                case UBLOXCFG_TYPE_I2: val = dbValue.val_.I2; width =  7; min = INT16_MIN; max = INT16_MAX; break;
                case UBLOXCFG_TYPE_E4:
                case UBLOXCFG_TYPE_I4: val = dbValue.val_.I4; width = 12; min = INT32_MIN; max = INT32_MAX; break;
                case UBLOXCFG_TYPE_I8: val = dbValue.val_.I8; width = 21; min = INT64_MIN; max = INT64_MAX; break;
                default: break;
            }  // clang-format on

            ImGui::PushItemFlag(ImGuiItemFlags_ButtonRepeat, true);

            // Decrement
            const bool canDec = (val > min);
            ImGui::BeginDisabled(!canDec);
            if (ImGui::Button("-", GUI_VAR.iconSize)) {
                val--;
                changed = true;
            }
            ImGui::EndDisabled();

            ImGui::SameLine(0.0f, smallSpacing);

            // Increment
            const bool canInc = (val < max);
            ImGui::BeginDisabled(!canInc);
            if (ImGui::Button("+", GUI_VAR.iconSize)) {
                val++;
                changed = true;
            }
            ImGui::EndDisabled();

            ImGui::PopItemFlag(); // repeat

            ImGui::SameLine();

            // Value
            ImGui::SetNextItemWidth(GUI_VAR.charSizeX * width);
            if (ImGui::InputScalar("##Value", ImGuiDataType_S64, &val, NULL, NULL, "%" PRIi64)) {
                changed = true;
            }

            // Constant selection
            if (ImGui::BeginPopupContextItem("Constants")) {
                char str[100];
                if (dbItem.def_ && (dbItem.def_->nConsts > 0)) {
                    ImGui::Separator();
                    bool valValid = false;
                    for (int ix = 0; ix < dbItem.def_->nConsts; ix++) {
                        const UBLOXCFG_CONST_t* c = &dbItem.def_->consts[ix];
                        std::snprintf(str, sizeof(str), "%-20s (%s)", c->name, c->value);
                        if (ImGui::RadioButton(str, val == c->val.E)) {
                            val = c->val.E;
                            changed = true;
                        }
                        Gui::ItemTooltip(c->title);
                        if (val == c->val.E) {
                            valValid = true;
                        }
                    }
                    if (!valValid) {
                        ImGui::Separator();
                        std::snprintf(str, sizeof(str), "Undefined            (%" PRIi64 ")", val);
                        ImGui::RadioButton(str, true);
                    }
                } else {
                    snprintf(str, sizeof(str), "Minimum: %" PRIi64, min);
                    if (ImGui::RadioButton(str, val == min)) {
                        val = min;
                        changed = true;
                    }
                    snprintf(str, sizeof(str), "Maximum:  %" PRIi64, max);
                    if (ImGui::RadioButton(str, val == max)) {
                        val = max;
                        changed = true;
                    }
                    if (ImGui::RadioButton("Zero:     0", val == 0)) {
                        val = 0;
                        changed = true;
                    }
                }
                ImGui::EndPopup();
            }

            if (changed)
            {
                switch (dbItem.type_)
                {
                    case UBLOXCFG_TYPE_I1: dbValue.val_.I1 = (int8_t)val; break;
                    case UBLOXCFG_TYPE_I2: dbValue.val_.I2 = (int16_t)val; break;
                    case UBLOXCFG_TYPE_I4: dbValue.val_.I4 = (int32_t)val; break;
                    case UBLOXCFG_TYPE_I8: dbValue.val_.I8 = (int64_t)val; break;
                    case UBLOXCFG_TYPE_E1: dbValue.val_.E1 = (int8_t)val;  break;
                    case UBLOXCFG_TYPE_E2: dbValue.val_.E2 = (int16_t)val; break;
                    case UBLOXCFG_TYPE_E4: dbValue.val_.E4 = (int32_t)val; break;
                    default: break;
                }
            }
            break;
        }
        case UBLOXCFG_TYPE_U1:
        case UBLOXCFG_TYPE_U2:
        case UBLOXCFG_TYPE_U4:
        case UBLOXCFG_TYPE_U8:
        case UBLOXCFG_TYPE_X1:
        case UBLOXCFG_TYPE_X2:
        case UBLOXCFG_TYPE_X4:
        case UBLOXCFG_TYPE_X8: {
            uint64_t val = dbValue.val_.U1;
            uint64_t min = 0;
            uint64_t max = UINT8_MAX;
            int width = 4;
            const char* fmt = "%" PRIu64;
            bool hex = false;
            switch (dbItem.type_) {  // clang-format off
                case UBLOXCFG_TYPE_U1: break;
                case UBLOXCFG_TYPE_U2: val = dbValue.val_.U2; width =  6; max = UINT16_MAX; break;
                case UBLOXCFG_TYPE_U4: val = dbValue.val_.U4; width = 11; max = UINT32_MAX; break;
                case UBLOXCFG_TYPE_U8: val = dbValue.val_.U8; width = 21; max = UINT64_MAX; break;
                case UBLOXCFG_TYPE_X1: val = dbValue.val_.X1; width =  3; max = UINT8_MAX;  fmt = "0x%02" PRIx64;  hex = true; break;
                case UBLOXCFG_TYPE_X2: val = dbValue.val_.X2; width =  5; max = UINT16_MAX; fmt = "0x%04" PRIx64;  hex = true; break;
                case UBLOXCFG_TYPE_X4: val = dbValue.val_.X4; width =  9; max = UINT32_MAX; fmt = "0x%08" PRIx64;  hex = true; break;
                case UBLOXCFG_TYPE_X8: val = dbValue.val_.X8; width = 17; max = UINT64_MAX; fmt = "0x%016" PRIx64; hex = true; break;
                default: break;
            }  // clang-format on

            ImGui::PushItemFlag(ImGuiItemFlags_ButtonRepeat, true);

            // Decrement
            const bool canDec = val > min;
            ImGui::BeginDisabled(!canDec);
            if (ImGui::Button("-", GUI_VAR.iconSize))
            {
                val--;
                changed = true;
            }
            ImGui::EndDisabled();

            ImGui::SameLine(0.0f, smallSpacing);

            // Increment
            const bool canInc = (val < max);
            ImGui::BeginDisabled(!canInc);
            if (ImGui::Button("+", GUI_VAR.iconSize))
            {
                val++;
                changed = true;
            }
            ImGui::EndDisabled();

            ImGui::PopItemFlag(); // repeat

            ImGui::SameLine();

            // Value
            ImGui::SetNextItemWidth(GUI_VAR.charSizeX * width);
            if (hex ?  // clang-format off
                 ImGui::InputScalar("##Value", ImGuiDataType_U64, &val, NULL, NULL, &fmt[2], ImGuiInputTextFlags_CharsHexadecimal) : // FIXME: doesn't work with fmt "0x..."
                 ImGui::InputScalar("##Value", ImGuiDataType_U64, &val, NULL, NULL, fmt)
                 /*ImGui::DragScalar("##Value", type, &val, dragSpeed, NULL, NULL, fmt)*/ ) // FIXME: DragScalar() doesn't work with hex
            {  // clang-format on
                changed = true;
            }

            // Constant selection
            if (ImGui::BeginPopupContextItem("Constants")) {
                char fmt2[100];
                snprintf(fmt2, sizeof(fmt2), "%%-20s (%s)", fmt);
                char str[100];
                if (dbItem.def_ && (dbItem.def_->nConsts > 0)) {
                    uint64_t allbits = 0;
                    ImGui::Separator();
                    for (int ix = 0; ix < dbItem.def_->nConsts; ix++) {
                        const UBLOXCFG_CONST_t* c = &dbItem.def_->consts[ix];
                        std::snprintf(str, sizeof(str), fmt2, c->name, c->val.X);
                        if (Gui::CheckboxFlagsX8(str, &val, c->val.X)) {
                            changed = true;
                        }
                        Gui::ItemTooltip(c->title);
                        allbits |= c->val.X;
                    }
                    ImGui::Separator();
                    std::snprintf(str, sizeof(str), fmt2, "All", allbits);
                    if (Gui::CheckboxFlagsX8(str, &val, allbits)) {
                        changed = true;
                    }
                    const uint64_t rembits = (~allbits) & val;
                    if (rembits != 0) {
                        std::snprintf(str, sizeof(str), fmt2, "Undefined", rembits);
                        if (Gui::CheckboxFlagsX8(str, &val, rembits)) {
                            changed = true;
                        }
                    }
                } else {
                    snprintf(str, sizeof(str), fmt2, "Minimum", min);
                    if (ImGui::RadioButton(str, val == min)) {
                        val = min;
                        changed = true;
                    }
                    snprintf(str, sizeof(str), fmt2, "Maximum", max);
                    if (ImGui::RadioButton(str, val == max)) {
                        val = max;
                        changed = true;
                    }
                }
                ImGui::EndPopup();
            }
            if (changed) {
                switch (dbItem.type_) {  // clang-format off
                    case UBLOXCFG_TYPE_U1: dbValue.val_.U1 = (uint8_t)val;  break;
                    case UBLOXCFG_TYPE_U2: dbValue.val_.U2 = (uint16_t)val; break;
                    case UBLOXCFG_TYPE_U4: dbValue.val_.U4 = (uint32_t)val; break;
                    case UBLOXCFG_TYPE_U8: dbValue.val_.U8 = (uint64_t)val; break;
                    case UBLOXCFG_TYPE_X1: dbValue.val_.X1 = (uint8_t)val;  break;
                    case UBLOXCFG_TYPE_X2: dbValue.val_.X2 = (uint16_t)val; break;
                    case UBLOXCFG_TYPE_X4: dbValue.val_.X4 = (uint32_t)val; break;
                    case UBLOXCFG_TYPE_X8: dbValue.val_.X8 = (uint64_t)val; break;
                    default: break;
                }  // clang-format on
            }
            break;
        }
        case UBLOXCFG_TYPE_R4:
        case UBLOXCFG_TYPE_R8: {
            double val = dbItem.type_ == UBLOXCFG_TYPE_R4 ? dbValue.val_.R4 : dbValue.val_.R8;
            const double max = dbItem.type_ == UBLOXCFG_TYPE_R4 ? FLT_MAX : DBL_MAX;
            const double min = -max;
            const double step = (val > 100.0 ? std::fabs(val * 1e-2) /* 1% */ : 1.0);

            ImGui::PushItemFlag(ImGuiItemFlags_ButtonRepeat, true);

            // Increment
            const bool canInc = (val < max);
            ImGui::BeginDisabled(!canInc);
            if (ImGui::Button("+", GUI_VAR.iconSize)) {
                val += step;
                val = std::floor(val + 0.5);
                changed = true;
            }
            ImGui::EndDisabled();

            ImGui::SameLine(0.0f, smallSpacing);

            // Decrement
            const bool canDec = (val > min);
            ImGui::BeginDisabled(!canDec);
            if (ImGui::Button("-", GUI_VAR.iconSize)) {
                val -= step;
                val = std::floor(val - 0.5);
                changed = true;
            }
            ImGui::EndDisabled();
            Gui::ItemTooltip("Decrement");

            ImGui::PopItemFlag(); // repeat

            ImGui::SameLine();

            // Value
            ImGui::SetNextItemWidth(GUI_VAR.charSizeX * 16);
            if (ImGui::InputScalar("##Value", ImGuiDataType_Double, &val, NULL, NULL, "%g")) {
                changed = true;
            }

            // Popup
            if (ImGui::BeginPopupContextItem("Constants")) {
                char str[100];
                std::snprintf(str, sizeof(str), "Min:  %g", min);
                if (ImGui::MenuItem(str)) {
                    val = min;
                    changed = true;
                }
                std::snprintf(str, sizeof(str), "Max:   %g", max);
                if (ImGui::MenuItem(str)) {
                    val = max;
                    changed = true;
                }
                if (ImGui::MenuItem("Zero: 0.0")) {
                    val = 0.0;
                    changed = true;
                }
                ImGui::Separator();
                ImGui::TextUnformatted("Binary hex:");
                if (dbItem.type_ == UBLOXCFG_TYPE_R8) {
                    uint64_t valHex;
                    std::memcpy(&valHex, &val, sizeof(valHex));
                    ImGui::SetNextItemWidth(GUI_VAR.charSizeX * 17);
                    if (ImGui::InputScalar("##DoubleHex", ImGuiDataType_U64, &valHex, NULL, NULL, "%016llx", ImGuiInputTextFlags_CharsHexadecimal)) {
                        std::memcpy(&val, &valHex, sizeof(val));
                        changed = true;
                    }
                } else {
                    float valFloat = (float)val;
                    uint32_t valHex;
                    std::memcpy(&valHex, &valFloat, sizeof(valHex));
                    ImGui::SetNextItemWidth(GUI_VAR.charSizeX * 9);
                    if (ImGui::InputScalar("##FloatHex", ImGuiDataType_U32, &valHex, NULL, NULL, "%08lx", ImGuiInputTextFlags_CharsHexadecimal)) {
                        std::memcpy(&valFloat, &valHex, sizeof(valFloat));
                        val = valFloat;
                        changed = true;
                    }
                }
                ImGui::EndPopup();
            }
            if (changed) {
                switch (dbItem.type_) {  // clang-format off
                    case UBLOXCFG_TYPE_R4: dbValue.val_.R4 = (float)val; break;
                    case UBLOXCFG_TYPE_R8: dbValue.val_.R8 =        val; break;
                    default: break;
                }  // clang-format on
            }
            break;
        }
        case UBLOXCFG_TYPE_L: {
            // ImGui::SameLine(0.0f, (3 * itemSpacing) + (2 * GUI_VAR.iconSize.x));
            if (ImGui::Checkbox("##L", &dbValue.val_.L)) {
                changed = true;
            }
            break;
        }
    }

    if (changed) {
        char str[100];
        if (ubloxcfg_stringifyValue(str, sizeof(str), dbItem.type_, dbItem.def_, &dbValue.val_)) {
            dbValue.str_ = str;
        }
        dbValue.valid_ = true;
    }

    return changed;
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataConfig::_DrawHeaderPopup(const Item::ValueIx valIx)
{
    Gui::TextTitle("Layer actions:");
    ImGui::SameLine();
    ImGui::TextUnformatted(Item::LAYER_NAMES[valIx]);
    ImGui::Separator();
    if ((valIx == Item::FLASH) || (valIx == Item::BBR)) {
        ImGui::BeginDisabled(itemsAvail_.empty());
        if (ImGui::Selectable("Clear all items")) {
            for (auto item : itemsAvail_) {
                item->editValues_[valIx] = {};
            }
            itemsDirty_ = true;
        }
        ImGui::EndDisabled();
        ImGui::BeginDisabled(itemsDisp_.empty());
        if (ImGui::Selectable("Clear displayed items")) {
            for (auto item : itemsDisp_) {
                if (item) { // nullptrs as group separators
                    item->editValues_[valIx] = {};
                }
            }
            itemsDirty_ = true;
        }
        ImGui::EndDisabled();
        ImGui::BeginDisabled(itemsEdit_.empty());
        if (ImGui::Selectable("Clear selected items")) {
            for (auto item : itemsEdit_) {
                item->editValues_[valIx] = {};
            }
            itemsDirty_ = true;
        }
        ImGui::EndDisabled();
        ImGui::Separator();
    }

    ImGui::BeginDisabled(itemsAvail_.empty());
    if (ImGui::BeginMenu("Copy all items to")) {
        for (const auto destIx : { Item::DEF, Item::FLASH, Item::BBR, Item::RAM }) {
            if ((valIx == destIx) || (destIx == Item::DEF)) {
                continue;
            }
            if (ImGui::Selectable(Item::LAYER_NAMES[destIx])) {
                for (auto item : itemsAvail_) {
                    item->editValues_[destIx] = item->editValues_[valIx];
                }
                itemsDirty_ = true;
            }
        }
        ImGui::EndMenu();
    }
    ImGui::EndDisabled();
    ImGui::BeginDisabled(itemsDisp_.empty());
    if (ImGui::BeginMenu("Copy displayed items to")) {
        for (const auto destIx : { Item::DEF, Item::FLASH, Item::BBR, Item::RAM }) {
            if ((valIx == destIx) || (destIx == Item::DEF)) {
                continue;
            }
            if (ImGui::Selectable(Item::LAYER_NAMES[destIx])) {
                for (auto item : itemsDisp_) {
                    if (item) {
                        item->editValues_[destIx] = item->editValues_[valIx];
                    }
                }
                itemsDirty_ = true;

            }
        }
        ImGui::EndMenu();
    }
    ImGui::EndDisabled();
    ImGui::BeginDisabled(itemsEdit_.empty());
    if (ImGui::BeginMenu("Copy selected items to")) {
        for (const auto destIx : { Item::DEF, Item::FLASH, Item::BBR, Item::RAM }) {
            if ((valIx == destIx) || (destIx == Item::DEF)) {
                continue;
            }
            if (ImGui::Selectable(Item::LAYER_NAMES[destIx])) {
                for (auto item : itemsEdit_) {
                    item->editValues_[destIx] = item->editValues_[valIx];
                }
                itemsDirty_ = true;

            }
        }
        ImGui::EndMenu();
    }
    ImGui::EndDisabled();
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataConfig::_SetState(const State state)
{
    switch (state) {
        case State::IDLE:
            DEBUG("%s IDLE", WinName().c_str());
            state_ = state;
            log_.AddNotice("Ready");
            break;
        case State::POLL:
            DEBUG("%s POLL", WinName().c_str());
            state_ = state;
            log_.AddNotice("Poll receiver database");
            break;
        case State::SAVE:
            DEBUG("%s SAVE", WinName().c_str());
            state_ = state;
            log_.AddNotice("Save configuration to receiver");
            break;
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataConfig::_PollStart()
{
    //itemsAll_.clear(); itemsDisp_.clear();
    poll_ = {};
    db_.StartValGetResp();
    _SetState(State::POLL);
    for (auto& item : itemsAll_) {
        item.dbValid_ = false;
    }
    itemsDirty_ = true;
}

void GuiWinDataConfig::_PollDone(const bool success)
{
    poll_ = {};
    _SetState(State::IDLE);
    if (!success) {
        db_.Clear();
        tabbarMain_.Switch(TB_LOG);
        return;
    }

    db_.EndValGetResp();
    const auto& dbItems = db_.GetItems();
    itemsAll_.reserve(dbItems.size());
    for (auto& dbItem : dbItems) {
        //if (dbItem.group_ != "CFG-RATE") { continue; } // XXX
        auto item = std::find_if(itemsAll_.begin(), itemsAll_.end(), [id=dbItem.id_](const auto& cand) { return cand.db_.id_ == id; });
        if (item != itemsAll_.end()) {
            item->UpdateDb(dbItem);
        } else {
            itemsAll_.emplace_back(dbItem);
        }
    }
    // std::copy(dbItems.begin(), dbItems.end(), std::back_inserter(itemsAll_));

    std::sort(itemsAll_.begin(), itemsAll_.end(), [](const auto& a, const auto& b) { return a.db_.name_ < b.db_.name_; });

    _Sync();
}

void GuiWinDataConfig::_PollLoop(const Time& now)
{
    switch (poll_.step_) {
        case Poll::Step::POLL: {
            log_.AddInfo("Poll layer %s, offset %" PRIuMAX, ubloxcfg_layerName(UbloxCfgDb::LAYERS[poll_.layerIx_]), poll_.offs_);
            const auto msg = MakeUbxCfgValgetAll(UbloxCfgDb::LAYERS[poll_.layerIx_], poll_.offs_);
            if (!msg) {
                log_.AddError("Failed making poll message");
                poll_.step_ = Poll::Step::FAIL;
                break;
            }
            input_->receiver_->Write(msg.value());
            poll_.step_ = Poll::Step::WAIT;
            poll_.to_ = now + Duration::FromSec(1.0);
            break;
        }
        case Poll::Step::WAIT:
            if (now > poll_.to_) {
                log_.AddWarning("Poll layer %s timeout at offs %" PRIuMAX, ubloxcfg_layerName(UbloxCfgDb::LAYERS[poll_.layerIx_]), poll_.offs_);
                poll_.step_ = Poll::Step::FAIL;
            }
            break;
        case Poll::Step::DONE:
            log_.AddNotice("Poll complete");
            _PollDone(true);
            break;
        case Poll::Step::FAIL:
            log_.AddError("Poll failed");
            _PollDone(false);
            break;
    }

}

void GuiWinDataConfig::_PollData(const DataMsg& msg)
{
    if (msg.origin_ != DataMsg::RCVD) {
        return;
    }

    switch (poll_.step_) {
        case Poll::Step::POLL:
            break;
        case Poll::Step::WAIT:
            // UBX-CFG-VALGET response
            if (msg.msg_.name_ == UBX_CFG_VALGET_STRID) {
                const auto n = db_.AddValGetResp(msg.msg_);
                log_.AddDebug("Poll layer %s got %" PRIuMAX " + %" PRIuMAX " items", ubloxcfg_layerName(UbloxCfgDb::LAYERS[poll_.layerIx_]), poll_.offs_, n);
                // More items in this layer
                if (n == UBX_CFG_VALGET_V1_MAX_KV) {
                    poll_.offs_ += n;
                    poll_.step_ = Poll::Step::POLL;
                }
                // Done for thhis layer
                else {
                    log_.AddInfo("Poll layer %s complete, got %" PRIuMAX " items",
                        ubloxcfg_layerName(UbloxCfgDb::LAYERS[poll_.layerIx_]), poll_.offs_ + n);
                    poll_.layerIx_++;
                    // More layers to poll
                    if (poll_.layerIx_ < UbloxCfgDb::LAYERS.size()) {
                        poll_.offs_ = 0;
                        poll_.step_ = Poll::Step::POLL;
                    }
                    // All layers complete
                    else {
                        poll_.step_ = Poll::Step::DONE;
                    }
                }
            }
            // else if (_IsAck(msg.msg_, UBX_CFG_VALGET_MSGID)) {
            //     DEBUG("ack");
            // }
            else if (_IsNakCfg(msg.msg_, UBX_CFG_VALGET_MSGID)) {
                // Flash and BBR layers may be empty
                if ((poll_.offs_ == 0) && ((UbloxCfgDb::LAYERS[poll_.layerIx_] == UBLOXCFG_LAYER_FLASH) || (UbloxCfgDb::LAYERS[poll_.layerIx_] == UBLOXCFG_LAYER_BBR))) {
                    log_.AddInfo("Poll layer %s no data", ubloxcfg_layerName(UbloxCfgDb::LAYERS[poll_.layerIx_]));
                    poll_.layerIx_++;
                    if (poll_.layerIx_ < UbloxCfgDb::LAYERS.size()) {
                        poll_.offs_ = 0;
                        poll_.step_ = Poll::Step::POLL;
                    } else {
                        poll_.step_ = Poll::Step::DONE;
                    }
                } else {
                    log_.AddWarning("Poll layer %s fail at offs %" PRIuMAX, ubloxcfg_layerName(UbloxCfgDb::LAYERS[poll_.layerIx_]), poll_.offs_);
                    poll_.step_ = Poll::Step::FAIL;
                }
            }
            break;
        case Poll::Step::DONE:
        case Poll::Step::FAIL:
            break;
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataConfig::_SaveStart()
{
    save_ = {};

    // Delete from Flash, BBR
    for (std::size_t ix : { Item::FLASH, Item::BBR }) {
        std::vector<uint32_t> ids;
        for (auto& item : itemsAll_) {
            if (item.valuesEdited_[ix] && !item.editValues_[ix].valid_) {
                DEBUG("delete %s from %" PRIuMAX, item.db_.name_.c_str(), ix);
                ids.push_back(item.db_.id_);
            }
        }
        if (!ids.empty()) {
            const auto msgs = MakeUbxCfgValdel({ UbloxCfgDb::LAYERS[ix] }, ids);
            std::copy(msgs.begin(), msgs.end(), std::back_inserter(save_.msgs_));
        }
    }

    // Set in Flash, BBR, RAM
    for (std::size_t ix : { Item::FLASH, Item::BBR, Item::RAM }) {
        std::vector<UBLOXCFG_KEYVAL_t> kvs;
        for (auto& item : itemsAll_) {
            if (item.valuesEdited_[ix] && item.editValues_[ix].valid_) {
                DEBUG("set %s in %" PRIuMAX, item.db_.name_.c_str(), ix);
                kvs.push_back({ .id = item.db_.id_, item.editValues_[ix].val_ });
            }
        }
        if (!kvs.empty()) {
            const auto msgs = MakeUbxCfgValset({ UbloxCfgDb::LAYERS[ix] }, kvs);
            std::copy(msgs.begin(), msgs.end(), std::back_inserter(save_.msgs_));
        }
    }

    DEBUG("have %" PRIuMAX " msgs", save_.msgs_.size());
    for (auto& msg : save_.msgs_) {
        DEBUG("- %s: %s", msg.name_.c_str(), msg.info_.c_str());
    }

    if (!save_.msgs_.empty()) {
        _SetState(State::SAVE);
    }
}

void GuiWinDataConfig::_SaveDone(const bool success)
{
    save_ = {};
    if (!success) {
        _SetState(State::IDLE);
        tabbarMain_.Switch(TB_LOG);
        return;
    }

    _PollStart();
}

void GuiWinDataConfig::_SaveLoop(const Time& now)
{
    switch (save_.step_) {
        case Save::Step::SEND: {
            const auto& msg = save_.msgs_[save_.ix_];
            log_.AddInfo("Send %s: %s", msg.name_.c_str(), msg.info_.c_str());
            input_->receiver_->Write(msg);
            save_.step_ = Save::Step::WAIT;
            save_.msgId_ = UbxMsgId(msg.Data());
            save_.to_ = now + Duration::FromSec(1.0);
            break;
        }
        case Save::Step::WAIT:
            if (now > save_.to_) {
                log_.AddWarning("Timeout saving configuration changes");
                save_.step_ = Save::Step::FAIL;
            }
            break;
        case Save::Step::DONE:
            log_.AddNotice("Configuration changes saved");
            _SaveDone(true);
            break;
        case Save::Step::FAIL:
            log_.AddError("Failed saving configuration changes");
            _SaveDone(false);
            break;
    }
}

void GuiWinDataConfig::_SaveData(const DataMsg& msg)
{
    if (msg.origin_ != DataMsg::RCVD) {
        return;
    }

    switch (save_.step_) {
        case Save::Step::SEND:
            break;
        case Save::Step::WAIT:
            // Success
            if (_IsAckCfg(msg.msg_, save_.msgId_)) {
                log_.AddDebug("%s %s", msg.msg_.name_.c_str(), msg.msg_.info_.c_str());
                save_.ix_++;
                if (save_.ix_ < save_.msgs_.size()) {
                    save_.step_ = Save::Step::SEND;
                } else {
                    log_.AddInfo("Completed saving configuration");
                    save_.step_ = Save::Step::DONE;
                }
            }
            // Fail
            else if (_IsNakCfg(msg.msg_, save_.msgId_)) {
                log_.AddWarning("Receiver rejected %s (%s)", save_.msgs_[save_.ix_].name_.c_str(),  msg.msg_.info_.c_str());
                save_.step_ = Save::Step::FAIL;
            }
            break;
        case Save::Step::DONE:
        case Save::Step::FAIL:
            break;
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataConfig::_SetView(const std::size_t viewIx)
{
    const auto& view = VIEWS[std::min(viewIx, VIEWS.size() - 1)];

    const std::vector filters = StrSplit(view.filter_, ",");

    for (auto& item: itemsAll_) {
        if (view.valIx_ != Item::DEF) {
            item.show_ = item.editValues_[view.valIx_].valid_;
            continue;
        }
        item.show_ = filters.empty();
        for (auto& filter : filters) {
            if (StrStartsWith(item.db_.name_, filter)) {
                item.show_ = (!view.filter2_ || StrEndsWith(item.db_.name_, view.filter2_));
                break;
            }
        }
    }

    viewIx_ = viewIx;

    if (viewIx_ > 0) {
        cfg_.showHidden = false;
        cfg_.showSelected = false;
        filter_.SetFilterStr("");
    }

    itemsDirty_ = true;
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinDataConfig::_Sync()
{
    const bool haveFilter = filter_.IsActive();
    const bool highlight = haveFilter && filter_.IsHighlight();

    itemsAvail_.clear();
    itemsDisp_.clear();
    itemsEdit_.clear();
    itemsDisp_.reserve(itemsAll_.size());
    itemsPending_ = 0;
    uint32_t groupId_ = 0;
    for (auto& item : itemsAll_) {
        if (!item.dbValid_) {
            continue;
        }
        itemsAvail_.push_back(&item);

        if (!item.show_) {
            continue;
        }

        if (!cfg_.showHidden && !item.db_.def_) {
            continue;
        }

        if (cfg_.showSelected && !item.selected_) {
            continue;
        }

        item.Sync();
        itemsPending_ += item.numEdited_;

        // Restore selected items... (see below)
        if (cfg_.selectedItems.count(item.db_.name_) > 0) {
            item.selected_ = true;
        }

        item.filterMatch_ = (!haveFilter || filter_.Match(item.search_));
        if (item.filterMatch_ || highlight) {

            if (groupId_ != item.db_.groupId_) {
                if (!itemsDisp_.empty()) {
                    itemsDisp_.push_back(nullptr); // dummy entry, see _DrawItems();
                }
                groupId_ = item.db_.groupId_;
            }

            itemsDisp_.push_back(&item);
            // if (item.selected_) {
            //     itemsEdit_.push_back(&item);
            // }
        }

        if (item.selected_) {
            itemsEdit_.push_back(&item);
        }

#ifndef NDEBUG // clang-format off
        static std::size_t n;
        item.debug_ = "**Debug info** " + std::to_string(n++) + " \n";
        item.debug_ += "search:       " + item.search_ + "\n";
        item.debug_ += "selected:     " + std::string(ToStr(item.selected_)) + "\n";
        item.debug_ += "filterMatch:  " + std::string(ToStr(item.filterMatch_)) + "\n";
        item.debug_ += "              Default              Flash                BBR                  RAM\n";
        item.debug_ += "-------------------------------------------------------------------------------------------------\n";
        item.debug_ += Sprintf("currValues:   %-20.20s %-20.20s %-20.20s %-20.20s\n"
                               "       raw:   %-20" PRIu64 " %-20" PRIu64 " %-20" PRIu64 " %-20" PRIu64 "\n",
            item.currValues_[Item::DEF].str_.c_str(), item.currValues_[Item::FLASH].str_.c_str(), item.currValues_[Item::BBR].str_.c_str(), item.currValues_[Item::RAM].str_.c_str(),
            item.currValues_[Item::DEF].val_._raw, item.currValues_[Item::FLASH].val_._raw, item.currValues_[Item::BBR].val_._raw, item.currValues_[Item::RAM].val_._raw);
        item.debug_ += Sprintf("     valid:   %-20s %-20s %-20s %-20s \n",
            ToStr(item.currValues_[Item::DEF].valid_), ToStr(item.currValues_[Item::FLASH].valid_), ToStr(item.currValues_[Item::BBR].valid_), ToStr(item.currValues_[Item::RAM].valid_));
        item.debug_ += Sprintf("currNonDef:   %-20s %-20s %-20s %-20s \n",
            ToStr(item.currNonDefault_[Item::DEF]), ToStr(item.currNonDefault_[Item::FLASH]), ToStr(item.currNonDefault_[Item::BBR]), ToStr(item.currNonDefault_[Item::RAM]));
        item.debug_ += "-------------------------------------------------------------------------------------------------\n";
        item.debug_ += Sprintf("editValues:   %-20.20s %-20.20s %-20.20s %-20.20s\n"
                               "       raw:   %-20" PRIu64 " %-20" PRIu64 " %-20" PRIu64 " %-20" PRIu64 "\n",
            item.editValues_[Item::DEF].str_.c_str(), item.editValues_[Item::FLASH].str_.c_str(), item.editValues_[Item::BBR].str_.c_str(), item.editValues_[Item::RAM].str_.c_str(),
            item.editValues_[Item::DEF].val_._raw, item.editValues_[Item::FLASH].val_._raw, item.editValues_[Item::BBR].val_._raw, item.editValues_[Item::RAM].val_._raw);
        item.debug_ += Sprintf("     valid:   %-20s %-20s %-20s %-20s \n",
            ToStr(item.editValues_[Item::DEF].valid_), ToStr(item.editValues_[Item::FLASH].valid_), ToStr(item.editValues_[Item::BBR].valid_), ToStr(item.editValues_[Item::RAM].valid_));
        item.debug_ += Sprintf("editNonDef:   %-20s %-20s %-20s %-20s \n",
            ToStr(item.editNonDefault_[Item::DEF]), ToStr(item.editNonDefault_[Item::FLASH]), ToStr(item.editNonDefault_[Item::BBR]), ToStr(item.editNonDefault_[Item::RAM]));
        item.debug_ += "-------------------------------------------------------------------------------------------------\n";
        item.debug_ += Sprintf("valuesEdited: %-20s %-20s %-20s %-20s \n",
            ToStr(item.valuesEdited_[Item::DEF]), ToStr(item.valuesEdited_[Item::FLASH]), ToStr(item.valuesEdited_[Item::BBR]), ToStr(item.valuesEdited_[Item::RAM]));
#endif // clang-format on
    }

    // (see above) ...if we have items
    if (!itemsAll_.empty()) {
        cfg_.selectedItems.clear();
    }
}

// ---------------------------------------------------------------------------------------------------------------------

GuiWinDataConfig::Item::Item(const UbloxCfgDb::Item& dbItem)
{
    editValues_ = { { dbItem.valDefault_, dbItem.valFlash_, dbItem.valBbr_, dbItem.valRam_ } };
    UpdateDb(dbItem);
}

void GuiWinDataConfig::Item::UpdateDb(const UbloxCfgDb::Item& dbItem)
{
    db_ = dbItem;
    currValues_ = { { db_.valDefault_, db_.valFlash_, db_.valBbr_, db_.valRam_ } };
    search_ = Sprintf("%s 0x%" PRIx32 " %s 0x%" PRIx32 " %s %s %s %s %s %s %s %s", db_.name_.c_str(), db_.id_,
        db_.group_.c_str(), db_.groupId_, ubloxcfg_typeStr(db_.type_), db_.title_.c_str(), db_.unit_.c_str(),
        db_.scale_.c_str(), db_.valDefault_.str_.c_str(), db_.valFlash_.str_.c_str(), db_.valBbr_.str_.c_str(),
        db_.valRam_.str_.c_str());
    dbValid_ = true;
}

void GuiWinDataConfig::Item::Sync()
{
    const auto& defaultValue = currValues_[Item::DEF];
    numEdited_ = 0;
    for (int ix = Item::FLASH; ix <= Item::RAM; ix++) {
        auto& currValue = currValues_[ix];
        auto& editValue = editValues_[ix];
        currNonDefault_[ix] = currValue.valid_ && (currValue != defaultValue);
        editNonDefault_[ix] = editValue.valid_ && (editValue != defaultValue);
        valuesEdited_[ix] = (editValue != currValue);
        if (valuesEdited_[ix]) {
            numEdited_++;
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------

bool GuiWinDataConfig::_IsAckCfg(const ParserMsg& msg, const uint8_t msgId) const
{
    if ((msg.Size() == UBX_ACK_ACK_V0_SIZE) && (msg.name_ == UBX_ACK_ACK_STRID)) {
        return msg.data_[UBX_HEAD_SIZE + 1] == msgId;
    }
    return false;
}

bool GuiWinDataConfig::_IsNakCfg(const ParserMsg& msg, const uint8_t msgId) const
{
    if ((msg.Size() == UBX_ACK_NAK_V0_SIZE) && (msg.name_ == UBX_ACK_NAK_STRID)) {
        return msg.data_[UBX_HEAD_SIZE + 1] == msgId;
    }
    return false;
}

/* ****************************************************************************************************************** */
} // ffgui

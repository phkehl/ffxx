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

#include "ffgui_inc.hpp"
//
#include <cstring>
//
#include "GL/gl3w.h"
//
#include "gui_win_experiment.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

GuiWinExperiment::GuiWinExperiment() /* clang-format off */ :
    GuiWin("Experiments", { 80, 20, 0, 0 }, ImGuiWindowFlags_NoDocking),
    tabbar_           { WinName() },
    openFileDialog_   { WinName() + ".OpenFileDialog" },
    saveFileDialog_   { WinName() + ".SaveFileDialog" }
#if 0
    ,
    inDataSrc_        { "ExperimentSrc" },
    inDataLog_         { WinName() }
#endif
 // clang-format on
{
}

GuiWinExperiment::~GuiWinExperiment()
{
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinExperiment::DrawWindow()
{
    if (!_DrawWindowBegin()) {
        return;
    }

    if (tabbar_.Begin()) {
        tabbar_.Item("GuiWinFileDialog", [&]() { _DrawGuiWinFileDialog(); });
        tabbar_.Item("GuiNotify", [&]() { _DrawGuiNotify(); });
        tabbar_.Item("GuiWidgetMap", [&]() { _DrawGuiWidgetMap(); });
        tabbar_.Item("Matrix", [&]() { _DrawMatrix(); });
        tabbar_.Item("Data", [&]() { _DrawData(); });
        tabbar_.Item("Icons", [&]() { _DrawIcons(); });
        tabbar_.End();
    }

    _DrawWindowEnd();
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinExperiment::_DrawGuiWinFileDialog()
{
    if (ImGui::Button("Open a file...")) {
        if (!openFileDialog_.IsInit()) {
            openFileDialog_.InitDialog(GuiWinFileDialog::FILE_OPEN);
            openFileDialog_.SetDirectory("/usr/share/doc");
        } else {
            openFileDialog_.WinFocus();
        }
    }
    ImGui::SameLine();
    ImGui::Text("--> %s", openFilePath_.c_str());

    if (ImGui::Button("Save a file...")) {
        if (!saveFileDialog_.IsInit()) {
            saveFileDialog_.InitDialog(GuiWinFileDialog::FILE_SAVE);
            saveFileDialog_.SetFilename("saveme.txt");
            saveFileDialog_.WinSetTitle("blablabla...");
            // saveFileDialog_->SetConfirmOverwrite(false);
        } else {
            saveFileDialog_.WinFocus();
        }
    }
    ImGui::SameLine();
    ImGui::Text("--> %s", saveFilePath_.c_str());

    if (openFileDialog_.IsInit()) {
        if (openFileDialog_.DrawDialog()) {
            openFilePath_ = openFileDialog_.GetPath();
            DEBUG("open file done");
        }
    }
    if (saveFileDialog_.IsInit()) {
        if (saveFileDialog_.DrawDialog()) {
            saveFilePath_ = saveFileDialog_.GetPath();
            DEBUG("save file done");
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinExperiment::_DrawGuiNotify()
{
    if (ImGui::Button("Notice: title, text")) {
        GuiNotify::Notice("Hear, hear!", "blabla");
    }
    if (ImGui::Button("Notice: title, no text")) {
        GuiNotify::Notice("Hear, hear!", "");
    }
    if (ImGui::Button("Error: title, text")) {
        GuiNotify::Error("Ouch!", "blabla");
    }
    if (ImGui::Button("Warning: no title, text")) {
        GuiNotify::Warning("", "blabla (10s)", 10);
    }
    if (ImGui::Button("Success: title, looooong text")) {
        GuiNotify::Success("That worked!",
            "blabla blabla blabla blablablablablablablablablablablablablablablablablablablabla blabla blabla blabla "
            "blabla blabla blabla blabla blablablabla blabla blabla blabla blabla blabla blabla blabla blabla blabla "
            "blabla blabla blabla blabla blabla blabla blabla blabla blabla blabla blablablabla blabla blabla blabla "
            "blabla blabla blabla blabla blabla blabla blabla blabla blabla blabla blabla blabla blabla blabla blabla "
            "blabla blabla");
    }
    if (ImGui::Button("Message: title, no text")) {
        GuiNotify::Message("message", "");
    }
    if (ImGui::Button("Message: title, text")) {
        GuiNotify::Message("message", "text");
    }
    if (ImGui::Button("Message: no title, text")) {
        GuiNotify::Message("", "message");
    }
    if (ImGui::Button("Warning: no title, no text")) {
        GuiNotify::Warning("", "");
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinExperiment::_DrawGuiWidgetMap()
{
    if (map_) {
        if (ImGui::Button("stop")) {
            map_ = nullptr;
        }
    } else {
        if (ImGui::Button("start")) {
            map_ = std::make_unique<GuiWidgetMap>(WinName());
        }
    }

    if (map_) {
        if (map_->BeginDraw()) {
            map_->EndDraw();
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinExperiment::_DrawMatrix()
{
    if (!matrixInit_) {
        if (ImGui::Button("start")) {
            matrixInit_ = matrix_.Init();
        }
    } else {
        if (ImGui::Button("stop")) {
            matrix_.Destroy();
            matrixInit_ = false;
        }
    }

    if (matrixInit_) {
        const Vec2f pos = ImGui::GetCursorScreenPos();
        const Vec2f size = ImGui::GetContentRegionAvail();
        ImGui::Text("matrix %.0fx%.0f", size.x, size.y);

        if (framebuffer_.Begin(size.x, size.y)) {
            ImGui::TextUnformatted("framebuffer ok");
            ImGui::Text("frame %u", ImGui::GetFrameCount());

            // framebuffer_.Clear(1.0,0.0,0.0,1.0);
            matrix_.Animate();
            matrix_.Render(size.x, size.y);

            framebuffer_.End();
            void* tex = framebuffer_.GetTexture();
            if (tex) {
                ImGui::Text("texture %p", tex);
                ImGui::GetWindowDrawList()->AddImage(
                    ImTextureRef((ImTextureID)tex), pos, pos + size, ImVec2(0, 1), ImVec2(1, 0));
            } else {
                ImGui::TextUnformatted("no texture");
            }
        } else {
            ImGui::TextUnformatted("no framebuffer");
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinExperiment::_DrawData()
{
#if 0
    // Source
    ImGui::PushItemWidth(GUI_VAR.widgetOffs);
    ImGui::Text("Src: (ID 0x%016" PRIxMAX ")", inDataSrc_.id_);
    if (ImGui::InputText("##DataSrc", inDataSrc_.name_, sizeof(inDataSrc_.name_))) {
        inDataSrc_ = { inDataSrc_.name_ };
    }
    ImGui::PopItemWidth();
    ImGui::SameLine();
    if (ImGui::Button("Notify MSG (all)")) {
        ParserMsg msg;
        msg.name_ = Sprintf("MSG-%06" PRIuMAX, ++inputDataNotifyMsgCount_);
        inDataLog_.AddLine(
            Sprintf("CollectData (all) MSG %s: %s", inDataSrc_.name_, msg.name_.c_str()), C_C_BRIGHTCYAN());
        CollectData(std::make_shared<DataMsg>(msg, DataMsg::UNKNOWN, inDataSrc_));
    }
    ImGui::SameLine();
    if (ImGui::Button("Notify MSG (obs)")) {
        ParserMsg msg;
        msg.name_ = Sprintf("MSG-%06" PRIuMAX, ++inputDataNotifyMsgCount_);
        inDataLog_.AddLine(
            Sprintf("CollectData (obs) MSG %s: %s", inDataSrc_.name_, msg.name_.c_str()), C_C_BRIGHTCYAN());
        CollectData(std::make_shared<DataMsg>(msg, DataMsg::UNKNOWN, inDataSrc_, inputDataObs_));
    }

    // Observer
    ImGui::Text("Obs: (ID 0x%016" PRIxMAX ")", inputDataObs_.id_);
    ImGui::PushItemWidth(GUI_VAR.widgetOffs);
    if (ImGui::InputText("##InputDatObs", inputDataObs_.name_, sizeof(inputDataObs_.name_))) {
        inputDataObs_ = { inputDataObs_.name_ };
    }
    ImGui::PopItemWidth();
    ImGui::SameLine();
    if (ImGui::Button("Add observer")) {
        const bool ok = AddDataObserver(inDataSrc_, inputDataObs_,
            std::bind(&GuiWinExperiment::_DataObserver, this, std::placeholders::_1));
        inDataLog_.AddLine(Sprintf("AddDataObserver %s -> %s", inDataSrc_.name_, inputDataObs_.name_),
            ok ? C_C_BRIGHTGREEN() : C_C_BRIGHTRED());
    }
    ImGui::SameLine();
    if (ImGui::Button("Remove observer")) {
        const bool ok = RemoveDataObserver(inDataSrc_, inputDataObs_);
        inDataLog_.AddLine(Sprintf("RemoveDataObserver %s -> %s", inDataSrc_.name_, inputDataObs_.name_),
            ok ? C_C_BRIGHTGREEN() : C_C_BRIGHTRED());
    }

    ImGui::Separator();

    // if (ImGui::Button("DispatchData")) { DispatchData(); } // --> main loop

    ImGui::Text("inputDataLatest    %p (%d)", inputDataLatest_ ? inputDataLatest_.get() : nullptr,
        inputDataLatest_ ? (int)inputDataLatest_.use_count() : -1);
    ImGui::SameLine();
    if (ImGui::SmallButton("reset##1")) {
        inputDataLatest_.reset();
    }
    ImGui::Text("inputDataLatestMsg %p (%d)", inputDataLatestMsg_ ? inputDataLatestMsg_.get() : nullptr,
        inputDataLatestMsg_ ? (int)inputDataLatestMsg_.use_count() : -1);
    ImGui::SameLine();
    if (ImGui::SmallButton("reset##2")) {
        inputDataLatestMsg_.reset();
    }

    ImGui::Separator();

    ImGui::Text("Log:");
    inDataLog_.DrawLog();
#endif
}

#if 0
void GuiWinExperiment::_DataObserver(const DataPtr& data)
{
    switch (data->type_) {
        case DataType::MSG:
            inDataLog_.AddLine(
                Sprintf("Observer: MSG %s from %016" PRIxMAX "(%s) -> %016" PRIxMAX " (%s): %s",
                    data->time_.StrIsoTime(3).c_str(), data->src_.id_, data->src_.name_, data->dst_.id_,
                    data->dst_.name_, dynamic_cast<const DataMsg&>(*data).msg_.name_.c_str()),
                C_C_WHITE());
            break;
        case DataType::EPOCH:
            break;
    }
    inputDataLatest_ = data;
    inputDataLatestMsg_ = DataPtrToDataMsgPtr(data);
}
#endif

// ---------------------------------------------------------------------------------------------------------------------

static const struct
{
    const char* icon;
    const char* name;
} ICONS[] = {
    { ICON_FK_GLASS, "ICON_FK_GLASS" },
    { ICON_FK_MUSIC, "ICON_FK_MUSIC" },
    { ICON_FK_SEARCH, "ICON_FK_SEARCH" },
    { ICON_FK_ENVELOPE_O, "ICON_FK_ENVELOPE_O" },
    { ICON_FK_HEART, "ICON_FK_HEART" },
    { ICON_FK_STAR, "ICON_FK_STAR" },
    { ICON_FK_STAR_O, "ICON_FK_STAR_O" },
    { ICON_FK_USER, "ICON_FK_USER" },
    { ICON_FK_FILM, "ICON_FK_FILM" },
    { ICON_FK_TH_LARGE, "ICON_FK_TH_LARGE" },
    { ICON_FK_TH, "ICON_FK_TH" },
    { ICON_FK_TH_LIST, "ICON_FK_TH_LIST" },
    { ICON_FK_CHECK, "ICON_FK_CHECK" },
    { ICON_FK_TIMES, "ICON_FK_TIMES" },
    { ICON_FK_SEARCH_PLUS, "ICON_FK_SEARCH_PLUS" },
    { ICON_FK_SEARCH_MINUS, "ICON_FK_SEARCH_MINUS" },
    { ICON_FK_POWER_OFF, "ICON_FK_POWER_OFF" },
    { ICON_FK_SIGNAL, "ICON_FK_SIGNAL" },
    { ICON_FK_COG, "ICON_FK_COG" },
    { ICON_FK_TRASH_O, "ICON_FK_TRASH_O" },
    { ICON_FK_HOME, "ICON_FK_HOME" },
    { ICON_FK_FILE_O, "ICON_FK_FILE_O" },
    { ICON_FK_CLOCK_O, "ICON_FK_CLOCK_O" },
    { ICON_FK_ROAD, "ICON_FK_ROAD" },
    { ICON_FK_DOWNLOAD, "ICON_FK_DOWNLOAD" },
    { ICON_FK_ARROW_CIRCLE_O_DOWN, "ICON_FK_ARROW_CIRCLE_O_DOWN" },
    { ICON_FK_ARROW_CIRCLE_O_UP, "ICON_FK_ARROW_CIRCLE_O_UP" },
    { ICON_FK_INBOX, "ICON_FK_INBOX" },
    { ICON_FK_PLAY_CIRCLE_O, "ICON_FK_PLAY_CIRCLE_O" },
    { ICON_FK_REPEAT, "ICON_FK_REPEAT" },
    { ICON_FK_REFRESH, "ICON_FK_REFRESH" },
    { ICON_FK_LIST_ALT, "ICON_FK_LIST_ALT" },
    { ICON_FK_LOCK, "ICON_FK_LOCK" },
    { ICON_FK_FLAG, "ICON_FK_FLAG" },
    { ICON_FK_HEADPHONES, "ICON_FK_HEADPHONES" },
    { ICON_FK_VOLUME_OFF, "ICON_FK_VOLUME_OFF" },
    { ICON_FK_VOLUME_DOWN, "ICON_FK_VOLUME_DOWN" },
    { ICON_FK_VOLUME_UP, "ICON_FK_VOLUME_UP" },
    { ICON_FK_QRCODE, "ICON_FK_QRCODE" },
    { ICON_FK_BARCODE, "ICON_FK_BARCODE" },
    { ICON_FK_TAG, "ICON_FK_TAG" },
    { ICON_FK_TAGS, "ICON_FK_TAGS" },
    { ICON_FK_BOOK, "ICON_FK_BOOK" },
    { ICON_FK_BOOKMARK, "ICON_FK_BOOKMARK" },
    { ICON_FK_PRINT, "ICON_FK_PRINT" },
    { ICON_FK_CAMERA, "ICON_FK_CAMERA" },
    { ICON_FK_FONT, "ICON_FK_FONT" },
    { ICON_FK_BOLD, "ICON_FK_BOLD" },
    { ICON_FK_ITALIC, "ICON_FK_ITALIC" },
    { ICON_FK_TEXT_HEIGHT, "ICON_FK_TEXT_HEIGHT" },
    { ICON_FK_TEXT_WIDTH, "ICON_FK_TEXT_WIDTH" },
    { ICON_FK_ALIGN_LEFT, "ICON_FK_ALIGN_LEFT" },
    { ICON_FK_ALIGN_CENTER, "ICON_FK_ALIGN_CENTER" },
    { ICON_FK_ALIGN_RIGHT, "ICON_FK_ALIGN_RIGHT" },
    { ICON_FK_ALIGN_JUSTIFY, "ICON_FK_ALIGN_JUSTIFY" },
    { ICON_FK_LIST, "ICON_FK_LIST" },
    { ICON_FK_OUTDENT, "ICON_FK_OUTDENT" },
    { ICON_FK_INDENT, "ICON_FK_INDENT" },
    { ICON_FK_VIDEO_CAMERA, "ICON_FK_VIDEO_CAMERA" },
    { ICON_FK_PICTURE_O, "ICON_FK_PICTURE_O" },
    { ICON_FK_PENCIL, "ICON_FK_PENCIL" },
    { ICON_FK_MAP_MARKER, "ICON_FK_MAP_MARKER" },
    { ICON_FK_ADJUST, "ICON_FK_ADJUST" },
    { ICON_FK_TINT, "ICON_FK_TINT" },
    { ICON_FK_PENCIL_SQUARE_O, "ICON_FK_PENCIL_SQUARE_O" },
    { ICON_FK_SHARE_SQUARE_O, "ICON_FK_SHARE_SQUARE_O" },
    { ICON_FK_CHECK_SQUARE_O, "ICON_FK_CHECK_SQUARE_O" },
    { ICON_FK_ARROWS, "ICON_FK_ARROWS" },
    { ICON_FK_STEP_BACKWARD, "ICON_FK_STEP_BACKWARD" },
    { ICON_FK_FAST_BACKWARD, "ICON_FK_FAST_BACKWARD" },
    { ICON_FK_BACKWARD, "ICON_FK_BACKWARD" },
    { ICON_FK_PLAY, "ICON_FK_PLAY" },
    { ICON_FK_PAUSE, "ICON_FK_PAUSE" },
    { ICON_FK_STOP, "ICON_FK_STOP" },
    { ICON_FK_FORWARD, "ICON_FK_FORWARD" },
    { ICON_FK_FAST_FORWARD, "ICON_FK_FAST_FORWARD" },
    { ICON_FK_STEP_FORWARD, "ICON_FK_STEP_FORWARD" },
    { ICON_FK_EJECT, "ICON_FK_EJECT" },
    { ICON_FK_CHEVRON_LEFT, "ICON_FK_CHEVRON_LEFT" },
    { ICON_FK_CHEVRON_RIGHT, "ICON_FK_CHEVRON_RIGHT" },
    { ICON_FK_PLUS_CIRCLE, "ICON_FK_PLUS_CIRCLE" },
    { ICON_FK_MINUS_CIRCLE, "ICON_FK_MINUS_CIRCLE" },
    { ICON_FK_TIMES_CIRCLE, "ICON_FK_TIMES_CIRCLE" },
    { ICON_FK_CHECK_CIRCLE, "ICON_FK_CHECK_CIRCLE" },
    { ICON_FK_QUESTION_CIRCLE, "ICON_FK_QUESTION_CIRCLE" },
    { ICON_FK_INFO_CIRCLE, "ICON_FK_INFO_CIRCLE" },
    { ICON_FK_CROSSHAIRS, "ICON_FK_CROSSHAIRS" },
    { ICON_FK_TIMES_CIRCLE_O, "ICON_FK_TIMES_CIRCLE_O" },
    { ICON_FK_CHECK_CIRCLE_O, "ICON_FK_CHECK_CIRCLE_O" },
    { ICON_FK_BAN, "ICON_FK_BAN" },
    { ICON_FK_ARROW_LEFT, "ICON_FK_ARROW_LEFT" },
    { ICON_FK_ARROW_RIGHT, "ICON_FK_ARROW_RIGHT" },
    { ICON_FK_ARROW_UP, "ICON_FK_ARROW_UP" },
    { ICON_FK_ARROW_DOWN, "ICON_FK_ARROW_DOWN" },
    { ICON_FK_SHARE, "ICON_FK_SHARE" },
    { ICON_FK_EXPAND, "ICON_FK_EXPAND" },
    { ICON_FK_COMPRESS, "ICON_FK_COMPRESS" },
    { ICON_FK_PLUS, "ICON_FK_PLUS" },
    { ICON_FK_MINUS, "ICON_FK_MINUS" },
    { ICON_FK_ASTERISK, "ICON_FK_ASTERISK" },
    { ICON_FK_EXCLAMATION_CIRCLE, "ICON_FK_EXCLAMATION_CIRCLE" },
    { ICON_FK_GIFT, "ICON_FK_GIFT" },
    { ICON_FK_LEAF, "ICON_FK_LEAF" },
    { ICON_FK_FIRE, "ICON_FK_FIRE" },
    { ICON_FK_EYE, "ICON_FK_EYE" },
    { ICON_FK_EYE_SLASH, "ICON_FK_EYE_SLASH" },
    { ICON_FK_EXCLAMATION_TRIANGLE, "ICON_FK_EXCLAMATION_TRIANGLE" },
    { ICON_FK_PLANE, "ICON_FK_PLANE" },
    { ICON_FK_CALENDAR, "ICON_FK_CALENDAR" },
    { ICON_FK_RANDOM, "ICON_FK_RANDOM" },
    { ICON_FK_COMMENT, "ICON_FK_COMMENT" },
    { ICON_FK_MAGNET, "ICON_FK_MAGNET" },
    { ICON_FK_CHEVRON_UP, "ICON_FK_CHEVRON_UP" },
    { ICON_FK_CHEVRON_DOWN, "ICON_FK_CHEVRON_DOWN" },
    { ICON_FK_RETWEET, "ICON_FK_RETWEET" },
    { ICON_FK_SHOPPING_CART, "ICON_FK_SHOPPING_CART" },
    { ICON_FK_FOLDER, "ICON_FK_FOLDER" },
    { ICON_FK_FOLDER_OPEN, "ICON_FK_FOLDER_OPEN" },
    { ICON_FK_ARROWS_V, "ICON_FK_ARROWS_V" },
    { ICON_FK_ARROWS_H, "ICON_FK_ARROWS_H" },
    { ICON_FK_BAR_CHART, "ICON_FK_BAR_CHART" },
    { ICON_FK_TWITTER_SQUARE, "ICON_FK_TWITTER_SQUARE" },
    { ICON_FK_FACEBOOK_SQUARE, "ICON_FK_FACEBOOK_SQUARE" },
    { ICON_FK_CAMERA_RETRO, "ICON_FK_CAMERA_RETRO" },
    { ICON_FK_KEY, "ICON_FK_KEY" },
    { ICON_FK_COGS, "ICON_FK_COGS" },
    { ICON_FK_COMMENTS, "ICON_FK_COMMENTS" },
    { ICON_FK_THUMBS_O_UP, "ICON_FK_THUMBS_O_UP" },
    { ICON_FK_THUMBS_O_DOWN, "ICON_FK_THUMBS_O_DOWN" },
    { ICON_FK_STAR_HALF, "ICON_FK_STAR_HALF" },
    { ICON_FK_HEART_O, "ICON_FK_HEART_O" },
    { ICON_FK_SIGN_OUT, "ICON_FK_SIGN_OUT" },
    { ICON_FK_LINKEDIN_SQUARE, "ICON_FK_LINKEDIN_SQUARE" },
    { ICON_FK_THUMB_TACK, "ICON_FK_THUMB_TACK" },
    { ICON_FK_EXTERNAL_LINK, "ICON_FK_EXTERNAL_LINK" },
    { ICON_FK_SIGN_IN, "ICON_FK_SIGN_IN" },
    { ICON_FK_TROPHY, "ICON_FK_TROPHY" },
    { ICON_FK_GITHUB_SQUARE, "ICON_FK_GITHUB_SQUARE" },
    { ICON_FK_UPLOAD, "ICON_FK_UPLOAD" },
    { ICON_FK_LEMON_O, "ICON_FK_LEMON_O" },
    { ICON_FK_PHONE, "ICON_FK_PHONE" },
    { ICON_FK_SQUARE_O, "ICON_FK_SQUARE_O" },
    { ICON_FK_BOOKMARK_O, "ICON_FK_BOOKMARK_O" },
    { ICON_FK_PHONE_SQUARE, "ICON_FK_PHONE_SQUARE" },
    { ICON_FK_TWITTER, "ICON_FK_TWITTER" },
    { ICON_FK_FACEBOOK, "ICON_FK_FACEBOOK" },
    { ICON_FK_GITHUB, "ICON_FK_GITHUB" },
    { ICON_FK_UNLOCK, "ICON_FK_UNLOCK" },
    { ICON_FK_CREDIT_CARD, "ICON_FK_CREDIT_CARD" },
    { ICON_FK_RSS, "ICON_FK_RSS" },
    { ICON_FK_HDD_O, "ICON_FK_HDD_O" },
    { ICON_FK_BULLHORN, "ICON_FK_BULLHORN" },
    { ICON_FK_BELL_O, "ICON_FK_BELL_O" },
    { ICON_FK_CERTIFICATE, "ICON_FK_CERTIFICATE" },
    { ICON_FK_HAND_O_RIGHT, "ICON_FK_HAND_O_RIGHT" },
    { ICON_FK_HAND_O_LEFT, "ICON_FK_HAND_O_LEFT" },
    { ICON_FK_HAND_O_UP, "ICON_FK_HAND_O_UP" },
    { ICON_FK_HAND_O_DOWN, "ICON_FK_HAND_O_DOWN" },
    { ICON_FK_ARROW_CIRCLE_LEFT, "ICON_FK_ARROW_CIRCLE_LEFT" },
    { ICON_FK_ARROW_CIRCLE_RIGHT, "ICON_FK_ARROW_CIRCLE_RIGHT" },
    { ICON_FK_ARROW_CIRCLE_UP, "ICON_FK_ARROW_CIRCLE_UP" },
    { ICON_FK_ARROW_CIRCLE_DOWN, "ICON_FK_ARROW_CIRCLE_DOWN" },
    { ICON_FK_GLOBE, "ICON_FK_GLOBE" },
    { ICON_FK_GLOBE_E, "ICON_FK_GLOBE_E" },
    { ICON_FK_GLOBE_W, "ICON_FK_GLOBE_W" },
    { ICON_FK_WRENCH, "ICON_FK_WRENCH" },
    { ICON_FK_TASKS, "ICON_FK_TASKS" },
    { ICON_FK_FILTER, "ICON_FK_FILTER" },
    { ICON_FK_BRIEFCASE, "ICON_FK_BRIEFCASE" },
    { ICON_FK_ARROWS_ALT, "ICON_FK_ARROWS_ALT" },
    { ICON_FK_USERS, "ICON_FK_USERS" },
    { ICON_FK_LINK, "ICON_FK_LINK" },
    { ICON_FK_CLOUD, "ICON_FK_CLOUD" },
    { ICON_FK_FLASK, "ICON_FK_FLASK" },
    { ICON_FK_SCISSORS, "ICON_FK_SCISSORS" },
    { ICON_FK_FILES_O, "ICON_FK_FILES_O" },
    { ICON_FK_PAPERCLIP, "ICON_FK_PAPERCLIP" },
    { ICON_FK_FLOPPY_O, "ICON_FK_FLOPPY_O" },
    { ICON_FK_SQUARE, "ICON_FK_SQUARE" },
    { ICON_FK_BARS, "ICON_FK_BARS" },
    { ICON_FK_LIST_UL, "ICON_FK_LIST_UL" },
    { ICON_FK_LIST_OL, "ICON_FK_LIST_OL" },
    { ICON_FK_STRIKETHROUGH, "ICON_FK_STRIKETHROUGH" },
    { ICON_FK_UNDERLINE, "ICON_FK_UNDERLINE" },
    { ICON_FK_TABLE, "ICON_FK_TABLE" },
    { ICON_FK_MAGIC, "ICON_FK_MAGIC" },
    { ICON_FK_TRUCK, "ICON_FK_TRUCK" },
    { ICON_FK_PINTEREST, "ICON_FK_PINTEREST" },
    { ICON_FK_PINTEREST_SQUARE, "ICON_FK_PINTEREST_SQUARE" },
    { ICON_FK_GOOGLE_PLUS_SQUARE, "ICON_FK_GOOGLE_PLUS_SQUARE" },
    { ICON_FK_GOOGLE_PLUS, "ICON_FK_GOOGLE_PLUS" },
    { ICON_FK_MONEY, "ICON_FK_MONEY" },
    { ICON_FK_CARET_DOWN, "ICON_FK_CARET_DOWN" },
    { ICON_FK_CARET_UP, "ICON_FK_CARET_UP" },
    { ICON_FK_CARET_LEFT, "ICON_FK_CARET_LEFT" },
    { ICON_FK_CARET_RIGHT, "ICON_FK_CARET_RIGHT" },
    { ICON_FK_COLUMNS, "ICON_FK_COLUMNS" },
    { ICON_FK_SORT, "ICON_FK_SORT" },
    { ICON_FK_SORT_DESC, "ICON_FK_SORT_DESC" },
    { ICON_FK_SORT_ASC, "ICON_FK_SORT_ASC" },
    { ICON_FK_ENVELOPE, "ICON_FK_ENVELOPE" },
    { ICON_FK_LINKEDIN, "ICON_FK_LINKEDIN" },
    { ICON_FK_UNDO, "ICON_FK_UNDO" },
    { ICON_FK_GAVEL, "ICON_FK_GAVEL" },
    { ICON_FK_TACHOMETER, "ICON_FK_TACHOMETER" },
    { ICON_FK_COMMENT_O, "ICON_FK_COMMENT_O" },
    { ICON_FK_COMMENTS_O, "ICON_FK_COMMENTS_O" },
    { ICON_FK_BOLT, "ICON_FK_BOLT" },
    { ICON_FK_SITEMAP, "ICON_FK_SITEMAP" },
    { ICON_FK_UMBRELLA, "ICON_FK_UMBRELLA" },
    { ICON_FK_CLIPBOARD, "ICON_FK_CLIPBOARD" },
    { ICON_FK_LIGHTBULB_O, "ICON_FK_LIGHTBULB_O" },
    { ICON_FK_EXCHANGE, "ICON_FK_EXCHANGE" },
    { ICON_FK_CLOUD_DOWNLOAD, "ICON_FK_CLOUD_DOWNLOAD" },
    { ICON_FK_CLOUD_UPLOAD, "ICON_FK_CLOUD_UPLOAD" },
    { ICON_FK_USER_MD, "ICON_FK_USER_MD" },
    { ICON_FK_STETHOSCOPE, "ICON_FK_STETHOSCOPE" },
    { ICON_FK_SUITCASE, "ICON_FK_SUITCASE" },
    { ICON_FK_BELL, "ICON_FK_BELL" },
    { ICON_FK_COFFEE, "ICON_FK_COFFEE" },
    { ICON_FK_CUTLERY, "ICON_FK_CUTLERY" },
    { ICON_FK_FILE_TEXT_O, "ICON_FK_FILE_TEXT_O" },
    { ICON_FK_BUILDING_O, "ICON_FK_BUILDING_O" },
    { ICON_FK_HOSPITAL_O, "ICON_FK_HOSPITAL_O" },
    { ICON_FK_AMBULANCE, "ICON_FK_AMBULANCE" },
    { ICON_FK_MEDKIT, "ICON_FK_MEDKIT" },
    { ICON_FK_FIGHTER_JET, "ICON_FK_FIGHTER_JET" },
    { ICON_FK_BEER, "ICON_FK_BEER" },
    { ICON_FK_H_SQUARE, "ICON_FK_H_SQUARE" },
    { ICON_FK_PLUS_SQUARE, "ICON_FK_PLUS_SQUARE" },
    { ICON_FK_ANGLE_DOUBLE_LEFT, "ICON_FK_ANGLE_DOUBLE_LEFT" },
    { ICON_FK_ANGLE_DOUBLE_RIGHT, "ICON_FK_ANGLE_DOUBLE_RIGHT" },
    { ICON_FK_ANGLE_DOUBLE_UP, "ICON_FK_ANGLE_DOUBLE_UP" },
    { ICON_FK_ANGLE_DOUBLE_DOWN, "ICON_FK_ANGLE_DOUBLE_DOWN" },
    { ICON_FK_ANGLE_LEFT, "ICON_FK_ANGLE_LEFT" },
    { ICON_FK_ANGLE_RIGHT, "ICON_FK_ANGLE_RIGHT" },
    { ICON_FK_ANGLE_UP, "ICON_FK_ANGLE_UP" },
    { ICON_FK_ANGLE_DOWN, "ICON_FK_ANGLE_DOWN" },
    { ICON_FK_DESKTOP, "ICON_FK_DESKTOP" },
    { ICON_FK_LAPTOP, "ICON_FK_LAPTOP" },
    { ICON_FK_TABLET, "ICON_FK_TABLET" },
    { ICON_FK_MOBILE, "ICON_FK_MOBILE" },
    { ICON_FK_CIRCLE_O, "ICON_FK_CIRCLE_O" },
    { ICON_FK_QUOTE_LEFT, "ICON_FK_QUOTE_LEFT" },
    { ICON_FK_QUOTE_RIGHT, "ICON_FK_QUOTE_RIGHT" },
    { ICON_FK_SPINNER, "ICON_FK_SPINNER" },
    { ICON_FK_CIRCLE, "ICON_FK_CIRCLE" },
    { ICON_FK_REPLY, "ICON_FK_REPLY" },
    { ICON_FK_GITHUB_ALT, "ICON_FK_GITHUB_ALT" },
    { ICON_FK_FOLDER_O, "ICON_FK_FOLDER_O" },
    { ICON_FK_FOLDER_OPEN_O, "ICON_FK_FOLDER_OPEN_O" },
    { ICON_FK_SMILE_O, "ICON_FK_SMILE_O" },
    { ICON_FK_FROWN_O, "ICON_FK_FROWN_O" },
    { ICON_FK_MEH_O, "ICON_FK_MEH_O" },
    { ICON_FK_GAMEPAD, "ICON_FK_GAMEPAD" },
    { ICON_FK_KEYBOARD_O, "ICON_FK_KEYBOARD_O" },
    { ICON_FK_FLAG_O, "ICON_FK_FLAG_O" },
    { ICON_FK_FLAG_CHECKERED, "ICON_FK_FLAG_CHECKERED" },
    { ICON_FK_TERMINAL, "ICON_FK_TERMINAL" },
    { ICON_FK_CODE, "ICON_FK_CODE" },
    { ICON_FK_REPLY_ALL, "ICON_FK_REPLY_ALL" },
    { ICON_FK_STAR_HALF_O, "ICON_FK_STAR_HALF_O" },
    { ICON_FK_LOCATION_ARROW, "ICON_FK_LOCATION_ARROW" },
    { ICON_FK_CROP, "ICON_FK_CROP" },
    { ICON_FK_CODE_FORK, "ICON_FK_CODE_FORK" },
    { ICON_FK_CHAIN_BROKEN, "ICON_FK_CHAIN_BROKEN" },
    { ICON_FK_QUESTION, "ICON_FK_QUESTION" },
    { ICON_FK_INFO, "ICON_FK_INFO" },
    { ICON_FK_EXCLAMATION, "ICON_FK_EXCLAMATION" },
    { ICON_FK_SUPERSCRIPT, "ICON_FK_SUPERSCRIPT" },
    { ICON_FK_SUBSCRIPT, "ICON_FK_SUBSCRIPT" },
    { ICON_FK_ERASER, "ICON_FK_ERASER" },
    { ICON_FK_PUZZLE_PIECE, "ICON_FK_PUZZLE_PIECE" },
    { ICON_FK_MICROPHONE, "ICON_FK_MICROPHONE" },
    { ICON_FK_MICROPHONE_SLASH, "ICON_FK_MICROPHONE_SLASH" },
    { ICON_FK_SHIELD, "ICON_FK_SHIELD" },
    { ICON_FK_CALENDAR_O, "ICON_FK_CALENDAR_O" },
    { ICON_FK_FIRE_EXTINGUISHER, "ICON_FK_FIRE_EXTINGUISHER" },
    { ICON_FK_ROCKET, "ICON_FK_ROCKET" },
    { ICON_FK_MAXCDN, "ICON_FK_MAXCDN" },
    { ICON_FK_CHEVRON_CIRCLE_LEFT, "ICON_FK_CHEVRON_CIRCLE_LEFT" },
    { ICON_FK_CHEVRON_CIRCLE_RIGHT, "ICON_FK_CHEVRON_CIRCLE_RIGHT" },
    { ICON_FK_CHEVRON_CIRCLE_UP, "ICON_FK_CHEVRON_CIRCLE_UP" },
    { ICON_FK_CHEVRON_CIRCLE_DOWN, "ICON_FK_CHEVRON_CIRCLE_DOWN" },
    { ICON_FK_HTML5, "ICON_FK_HTML5" },
    { ICON_FK_CSS3, "ICON_FK_CSS3" },
    { ICON_FK_ANCHOR, "ICON_FK_ANCHOR" },
    { ICON_FK_UNLOCK_ALT, "ICON_FK_UNLOCK_ALT" },
    { ICON_FK_BULLSEYE, "ICON_FK_BULLSEYE" },
    { ICON_FK_ELLIPSIS_H, "ICON_FK_ELLIPSIS_H" },
    { ICON_FK_ELLIPSIS_V, "ICON_FK_ELLIPSIS_V" },
    { ICON_FK_RSS_SQUARE, "ICON_FK_RSS_SQUARE" },
    { ICON_FK_PLAY_CIRCLE, "ICON_FK_PLAY_CIRCLE" },
    { ICON_FK_TICKET, "ICON_FK_TICKET" },
    { ICON_FK_MINUS_SQUARE, "ICON_FK_MINUS_SQUARE" },
    { ICON_FK_MINUS_SQUARE_O, "ICON_FK_MINUS_SQUARE_O" },
    { ICON_FK_LEVEL_UP, "ICON_FK_LEVEL_UP" },
    { ICON_FK_LEVEL_DOWN, "ICON_FK_LEVEL_DOWN" },
    { ICON_FK_CHECK_SQUARE, "ICON_FK_CHECK_SQUARE" },
    { ICON_FK_PENCIL_SQUARE, "ICON_FK_PENCIL_SQUARE" },
    { ICON_FK_EXTERNAL_LINK_SQUARE, "ICON_FK_EXTERNAL_LINK_SQUARE" },
    { ICON_FK_SHARE_SQUARE, "ICON_FK_SHARE_SQUARE" },
    { ICON_FK_COMPASS, "ICON_FK_COMPASS" },
    { ICON_FK_CARET_SQUARE_O_DOWN, "ICON_FK_CARET_SQUARE_O_DOWN" },
    { ICON_FK_CARET_SQUARE_O_UP, "ICON_FK_CARET_SQUARE_O_UP" },
    { ICON_FK_CARET_SQUARE_O_RIGHT, "ICON_FK_CARET_SQUARE_O_RIGHT" },
    { ICON_FK_EUR, "ICON_FK_EUR" },
    { ICON_FK_GBP, "ICON_FK_GBP" },
    { ICON_FK_USD, "ICON_FK_USD" },
    { ICON_FK_INR, "ICON_FK_INR" },
    { ICON_FK_JPY, "ICON_FK_JPY" },
    { ICON_FK_RUB, "ICON_FK_RUB" },
    { ICON_FK_KRW, "ICON_FK_KRW" },
    { ICON_FK_BTC, "ICON_FK_BTC" },
    { ICON_FK_FILE, "ICON_FK_FILE" },
    { ICON_FK_FILE_TEXT, "ICON_FK_FILE_TEXT" },
    { ICON_FK_SORT_ALPHA_ASC, "ICON_FK_SORT_ALPHA_ASC" },
    { ICON_FK_SORT_ALPHA_DESC, "ICON_FK_SORT_ALPHA_DESC" },
    { ICON_FK_SORT_AMOUNT_ASC, "ICON_FK_SORT_AMOUNT_ASC" },
    { ICON_FK_SORT_AMOUNT_DESC, "ICON_FK_SORT_AMOUNT_DESC" },
    { ICON_FK_SORT_NUMERIC_ASC, "ICON_FK_SORT_NUMERIC_ASC" },
    { ICON_FK_SORT_NUMERIC_DESC, "ICON_FK_SORT_NUMERIC_DESC" },
    { ICON_FK_THUMBS_UP, "ICON_FK_THUMBS_UP" },
    { ICON_FK_THUMBS_DOWN, "ICON_FK_THUMBS_DOWN" },
    { ICON_FK_YOUTUBE_SQUARE, "ICON_FK_YOUTUBE_SQUARE" },
    { ICON_FK_YOUTUBE, "ICON_FK_YOUTUBE" },
    { ICON_FK_XING, "ICON_FK_XING" },
    { ICON_FK_XING_SQUARE, "ICON_FK_XING_SQUARE" },
    { ICON_FK_YOUTUBE_PLAY, "ICON_FK_YOUTUBE_PLAY" },
    { ICON_FK_DROPBOX, "ICON_FK_DROPBOX" },
    { ICON_FK_STACK_OVERFLOW, "ICON_FK_STACK_OVERFLOW" },
    { ICON_FK_INSTAGRAM, "ICON_FK_INSTAGRAM" },
    { ICON_FK_FLICKR, "ICON_FK_FLICKR" },
    { ICON_FK_ADN, "ICON_FK_ADN" },
    { ICON_FK_BITBUCKET, "ICON_FK_BITBUCKET" },
    { ICON_FK_BITBUCKET_SQUARE, "ICON_FK_BITBUCKET_SQUARE" },
    { ICON_FK_TUMBLR, "ICON_FK_TUMBLR" },
    { ICON_FK_TUMBLR_SQUARE, "ICON_FK_TUMBLR_SQUARE" },
    { ICON_FK_LONG_ARROW_DOWN, "ICON_FK_LONG_ARROW_DOWN" },
    { ICON_FK_LONG_ARROW_UP, "ICON_FK_LONG_ARROW_UP" },
    { ICON_FK_LONG_ARROW_LEFT, "ICON_FK_LONG_ARROW_LEFT" },
    { ICON_FK_LONG_ARROW_RIGHT, "ICON_FK_LONG_ARROW_RIGHT" },
    { ICON_FK_APPLE, "ICON_FK_APPLE" },
    { ICON_FK_WINDOWS, "ICON_FK_WINDOWS" },
    { ICON_FK_ANDROID, "ICON_FK_ANDROID" },
    { ICON_FK_LINUX, "ICON_FK_LINUX" },
    { ICON_FK_DRIBBBLE, "ICON_FK_DRIBBBLE" },
    { ICON_FK_SKYPE, "ICON_FK_SKYPE" },
    { ICON_FK_FOURSQUARE, "ICON_FK_FOURSQUARE" },
    { ICON_FK_TRELLO, "ICON_FK_TRELLO" },
    { ICON_FK_FEMALE, "ICON_FK_FEMALE" },
    { ICON_FK_MALE, "ICON_FK_MALE" },
    { ICON_FK_GRATIPAY, "ICON_FK_GRATIPAY" },
    { ICON_FK_SUN_O, "ICON_FK_SUN_O" },
    { ICON_FK_MOON_O, "ICON_FK_MOON_O" },
    { ICON_FK_ARCHIVE, "ICON_FK_ARCHIVE" },
    { ICON_FK_BUG, "ICON_FK_BUG" },
    { ICON_FK_VK, "ICON_FK_VK" },
    { ICON_FK_WEIBO, "ICON_FK_WEIBO" },
    { ICON_FK_RENREN, "ICON_FK_RENREN" },
    { ICON_FK_PAGELINES, "ICON_FK_PAGELINES" },
    { ICON_FK_STACK_EXCHANGE, "ICON_FK_STACK_EXCHANGE" },
    { ICON_FK_ARROW_CIRCLE_O_RIGHT, "ICON_FK_ARROW_CIRCLE_O_RIGHT" },
    { ICON_FK_ARROW_CIRCLE_O_LEFT, "ICON_FK_ARROW_CIRCLE_O_LEFT" },
    { ICON_FK_CARET_SQUARE_O_LEFT, "ICON_FK_CARET_SQUARE_O_LEFT" },
    { ICON_FK_DOT_CIRCLE_O, "ICON_FK_DOT_CIRCLE_O" },
    { ICON_FK_WHEELCHAIR, "ICON_FK_WHEELCHAIR" },
    { ICON_FK_VIMEO_SQUARE, "ICON_FK_VIMEO_SQUARE" },
    { ICON_FK_TRY, "ICON_FK_TRY" },
    { ICON_FK_PLUS_SQUARE_O, "ICON_FK_PLUS_SQUARE_O" },
    { ICON_FK_SPACE_SHUTTLE, "ICON_FK_SPACE_SHUTTLE" },
    { ICON_FK_SLACK, "ICON_FK_SLACK" },
    { ICON_FK_ENVELOPE_SQUARE, "ICON_FK_ENVELOPE_SQUARE" },
    { ICON_FK_WORDPRESS, "ICON_FK_WORDPRESS" },
    { ICON_FK_OPENID, "ICON_FK_OPENID" },
    { ICON_FK_UNIVERSITY, "ICON_FK_UNIVERSITY" },
    { ICON_FK_GRADUATION_CAP, "ICON_FK_GRADUATION_CAP" },
    { ICON_FK_YAHOO, "ICON_FK_YAHOO" },
    { ICON_FK_GOOGLE, "ICON_FK_GOOGLE" },
    { ICON_FK_REDDIT, "ICON_FK_REDDIT" },
    { ICON_FK_REDDIT_SQUARE, "ICON_FK_REDDIT_SQUARE" },
    { ICON_FK_STUMBLEUPON_CIRCLE, "ICON_FK_STUMBLEUPON_CIRCLE" },
    { ICON_FK_STUMBLEUPON, "ICON_FK_STUMBLEUPON" },
    { ICON_FK_DELICIOUS, "ICON_FK_DELICIOUS" },
    { ICON_FK_DIGG, "ICON_FK_DIGG" },
    { ICON_FK_DRUPAL, "ICON_FK_DRUPAL" },
    { ICON_FK_JOOMLA, "ICON_FK_JOOMLA" },
    { ICON_FK_LANGUAGE, "ICON_FK_LANGUAGE" },
    { ICON_FK_FAX, "ICON_FK_FAX" },
    { ICON_FK_BUILDING, "ICON_FK_BUILDING" },
    { ICON_FK_CHILD, "ICON_FK_CHILD" },
    { ICON_FK_PAW, "ICON_FK_PAW" },
    { ICON_FK_SPOON, "ICON_FK_SPOON" },
    { ICON_FK_CUBE, "ICON_FK_CUBE" },
    { ICON_FK_CUBES, "ICON_FK_CUBES" },
    { ICON_FK_BEHANCE, "ICON_FK_BEHANCE" },
    { ICON_FK_BEHANCE_SQUARE, "ICON_FK_BEHANCE_SQUARE" },
    { ICON_FK_STEAM, "ICON_FK_STEAM" },
    { ICON_FK_STEAM_SQUARE, "ICON_FK_STEAM_SQUARE" },
    { ICON_FK_RECYCLE, "ICON_FK_RECYCLE" },
    { ICON_FK_CAR, "ICON_FK_CAR" },
    { ICON_FK_TAXI, "ICON_FK_TAXI" },
    { ICON_FK_TREE, "ICON_FK_TREE" },
    { ICON_FK_SPOTIFY, "ICON_FK_SPOTIFY" },
    { ICON_FK_DEVIANTART, "ICON_FK_DEVIANTART" },
    { ICON_FK_SOUNDCLOUD, "ICON_FK_SOUNDCLOUD" },
    { ICON_FK_DATABASE, "ICON_FK_DATABASE" },
    { ICON_FK_FILE_PDF_O, "ICON_FK_FILE_PDF_O" },
    { ICON_FK_FILE_WORD_O, "ICON_FK_FILE_WORD_O" },
    { ICON_FK_FILE_EXCEL_O, "ICON_FK_FILE_EXCEL_O" },
    { ICON_FK_FILE_POWERPOINT_O, "ICON_FK_FILE_POWERPOINT_O" },
    { ICON_FK_FILE_IMAGE_O, "ICON_FK_FILE_IMAGE_O" },
    { ICON_FK_FILE_ARCHIVE_O, "ICON_FK_FILE_ARCHIVE_O" },
    { ICON_FK_FILE_AUDIO_O, "ICON_FK_FILE_AUDIO_O" },
    { ICON_FK_FILE_VIDEO_O, "ICON_FK_FILE_VIDEO_O" },
    { ICON_FK_FILE_CODE_O, "ICON_FK_FILE_CODE_O" },
    { ICON_FK_VINE, "ICON_FK_VINE" },
    { ICON_FK_CODEPEN, "ICON_FK_CODEPEN" },
    { ICON_FK_JSFIDDLE, "ICON_FK_JSFIDDLE" },
    { ICON_FK_LIFE_RING, "ICON_FK_LIFE_RING" },
    { ICON_FK_CIRCLE_O_NOTCH, "ICON_FK_CIRCLE_O_NOTCH" },
    { ICON_FK_REBEL, "ICON_FK_REBEL" },
    { ICON_FK_EMPIRE, "ICON_FK_EMPIRE" },
    { ICON_FK_GIT_SQUARE, "ICON_FK_GIT_SQUARE" },
    { ICON_FK_GIT, "ICON_FK_GIT" },
    { ICON_FK_HACKER_NEWS, "ICON_FK_HACKER_NEWS" },
    { ICON_FK_TENCENT_WEIBO, "ICON_FK_TENCENT_WEIBO" },
    { ICON_FK_QQ, "ICON_FK_QQ" },
    { ICON_FK_WEIXIN, "ICON_FK_WEIXIN" },
    { ICON_FK_PAPER_PLANE, "ICON_FK_PAPER_PLANE" },
    { ICON_FK_PAPER_PLANE_O, "ICON_FK_PAPER_PLANE_O" },
    { ICON_FK_HISTORY, "ICON_FK_HISTORY" },
    { ICON_FK_CIRCLE_THIN, "ICON_FK_CIRCLE_THIN" },
    { ICON_FK_HEADER, "ICON_FK_HEADER" },
    { ICON_FK_PARAGRAPH, "ICON_FK_PARAGRAPH" },
    { ICON_FK_SLIDERS, "ICON_FK_SLIDERS" },
    { ICON_FK_SHARE_ALT, "ICON_FK_SHARE_ALT" },
    { ICON_FK_SHARE_ALT_SQUARE, "ICON_FK_SHARE_ALT_SQUARE" },
    { ICON_FK_BOMB, "ICON_FK_BOMB" },
    { ICON_FK_FUTBOL_O, "ICON_FK_FUTBOL_O" },
    { ICON_FK_TTY, "ICON_FK_TTY" },
    { ICON_FK_BINOCULARS, "ICON_FK_BINOCULARS" },
    { ICON_FK_PLUG, "ICON_FK_PLUG" },
    { ICON_FK_SLIDESHARE, "ICON_FK_SLIDESHARE" },
    { ICON_FK_TWITCH, "ICON_FK_TWITCH" },
    { ICON_FK_YELP, "ICON_FK_YELP" },
    { ICON_FK_NEWSPAPER_O, "ICON_FK_NEWSPAPER_O" },
    { ICON_FK_WIFI, "ICON_FK_WIFI" },
    { ICON_FK_CALCULATOR, "ICON_FK_CALCULATOR" },
    { ICON_FK_PAYPAL, "ICON_FK_PAYPAL" },
    { ICON_FK_GOOGLE_WALLET, "ICON_FK_GOOGLE_WALLET" },
    { ICON_FK_CC_VISA, "ICON_FK_CC_VISA" },
    { ICON_FK_CC_MASTERCARD, "ICON_FK_CC_MASTERCARD" },
    { ICON_FK_CC_DISCOVER, "ICON_FK_CC_DISCOVER" },
    { ICON_FK_CC_AMEX, "ICON_FK_CC_AMEX" },
    { ICON_FK_CC_PAYPAL, "ICON_FK_CC_PAYPAL" },
    { ICON_FK_CC_STRIPE, "ICON_FK_CC_STRIPE" },
    { ICON_FK_BELL_SLASH, "ICON_FK_BELL_SLASH" },
    { ICON_FK_BELL_SLASH_O, "ICON_FK_BELL_SLASH_O" },
    { ICON_FK_TRASH, "ICON_FK_TRASH" },
    { ICON_FK_COPYRIGHT, "ICON_FK_COPYRIGHT" },
    { ICON_FK_AT, "ICON_FK_AT" },
    { ICON_FK_EYEDROPPER, "ICON_FK_EYEDROPPER" },
    { ICON_FK_PAINT_BRUSH, "ICON_FK_PAINT_BRUSH" },
    { ICON_FK_BIRTHDAY_CAKE, "ICON_FK_BIRTHDAY_CAKE" },
    { ICON_FK_AREA_CHART, "ICON_FK_AREA_CHART" },
    { ICON_FK_PIE_CHART, "ICON_FK_PIE_CHART" },
    { ICON_FK_LINE_CHART, "ICON_FK_LINE_CHART" },
    { ICON_FK_LASTFM, "ICON_FK_LASTFM" },
    { ICON_FK_LASTFM_SQUARE, "ICON_FK_LASTFM_SQUARE" },
    { ICON_FK_TOGGLE_OFF, "ICON_FK_TOGGLE_OFF" },
    { ICON_FK_TOGGLE_ON, "ICON_FK_TOGGLE_ON" },
    { ICON_FK_BICYCLE, "ICON_FK_BICYCLE" },
    { ICON_FK_BUS, "ICON_FK_BUS" },
    { ICON_FK_IOXHOST, "ICON_FK_IOXHOST" },
    { ICON_FK_ANGELLIST, "ICON_FK_ANGELLIST" },
    { ICON_FK_CC, "ICON_FK_CC" },
    { ICON_FK_ILS, "ICON_FK_ILS" },
    { ICON_FK_MEANPATH, "ICON_FK_MEANPATH" },
    { ICON_FK_BUYSELLADS, "ICON_FK_BUYSELLADS" },
    { ICON_FK_CONNECTDEVELOP, "ICON_FK_CONNECTDEVELOP" },
    { ICON_FK_DASHCUBE, "ICON_FK_DASHCUBE" },
    { ICON_FK_FORUMBEE, "ICON_FK_FORUMBEE" },
    { ICON_FK_LEANPUB, "ICON_FK_LEANPUB" },
    { ICON_FK_SELLSY, "ICON_FK_SELLSY" },
    { ICON_FK_SHIRTSINBULK, "ICON_FK_SHIRTSINBULK" },
    { ICON_FK_SIMPLYBUILT, "ICON_FK_SIMPLYBUILT" },
    { ICON_FK_SKYATLAS, "ICON_FK_SKYATLAS" },
    { ICON_FK_CART_PLUS, "ICON_FK_CART_PLUS" },
    { ICON_FK_CART_ARROW_DOWN, "ICON_FK_CART_ARROW_DOWN" },
    { ICON_FK_DIAMOND, "ICON_FK_DIAMOND" },
    { ICON_FK_SHIP, "ICON_FK_SHIP" },
    { ICON_FK_USER_SECRET, "ICON_FK_USER_SECRET" },
    { ICON_FK_MOTORCYCLE, "ICON_FK_MOTORCYCLE" },
    { ICON_FK_STREET_VIEW, "ICON_FK_STREET_VIEW" },
    { ICON_FK_HEARTBEAT, "ICON_FK_HEARTBEAT" },
    { ICON_FK_VENUS, "ICON_FK_VENUS" },
    { ICON_FK_MARS, "ICON_FK_MARS" },
    { ICON_FK_MERCURY, "ICON_FK_MERCURY" },
    { ICON_FK_TRANSGENDER, "ICON_FK_TRANSGENDER" },
    { ICON_FK_TRANSGENDER_ALT, "ICON_FK_TRANSGENDER_ALT" },
    { ICON_FK_VENUS_DOUBLE, "ICON_FK_VENUS_DOUBLE" },
    { ICON_FK_MARS_DOUBLE, "ICON_FK_MARS_DOUBLE" },
    { ICON_FK_VENUS_MARS, "ICON_FK_VENUS_MARS" },
    { ICON_FK_MARS_STROKE, "ICON_FK_MARS_STROKE" },
    { ICON_FK_MARS_STROKE_V, "ICON_FK_MARS_STROKE_V" },
    { ICON_FK_MARS_STROKE_H, "ICON_FK_MARS_STROKE_H" },
    { ICON_FK_NEUTER, "ICON_FK_NEUTER" },
    { ICON_FK_GENDERLESS, "ICON_FK_GENDERLESS" },
    { ICON_FK_FACEBOOK_OFFICIAL, "ICON_FK_FACEBOOK_OFFICIAL" },
    { ICON_FK_PINTEREST_P, "ICON_FK_PINTEREST_P" },
    { ICON_FK_WHATSAPP, "ICON_FK_WHATSAPP" },
    { ICON_FK_SERVER, "ICON_FK_SERVER" },
    { ICON_FK_USER_PLUS, "ICON_FK_USER_PLUS" },
    { ICON_FK_USER_TIMES, "ICON_FK_USER_TIMES" },
    { ICON_FK_BED, "ICON_FK_BED" },
    { ICON_FK_VIACOIN, "ICON_FK_VIACOIN" },
    { ICON_FK_TRAIN, "ICON_FK_TRAIN" },
    { ICON_FK_SUBWAY, "ICON_FK_SUBWAY" },
    { ICON_FK_MEDIUM, "ICON_FK_MEDIUM" },
    { ICON_FK_MEDIUM_SQUARE, "ICON_FK_MEDIUM_SQUARE" },
    { ICON_FK_Y_COMBINATOR, "ICON_FK_Y_COMBINATOR" },
    { ICON_FK_OPTIN_MONSTER, "ICON_FK_OPTIN_MONSTER" },
    { ICON_FK_OPENCART, "ICON_FK_OPENCART" },
    { ICON_FK_EXPEDITEDSSL, "ICON_FK_EXPEDITEDSSL" },
    { ICON_FK_BATTERY_FULL, "ICON_FK_BATTERY_FULL" },
    { ICON_FK_BATTERY_THREE_QUARTERS, "ICON_FK_BATTERY_THREE_QUARTERS" },
    { ICON_FK_BATTERY_HALF, "ICON_FK_BATTERY_HALF" },
    { ICON_FK_BATTERY_QUARTER, "ICON_FK_BATTERY_QUARTER" },
    { ICON_FK_BATTERY_EMPTY, "ICON_FK_BATTERY_EMPTY" },
    { ICON_FK_MOUSE_POINTER, "ICON_FK_MOUSE_POINTER" },
    { ICON_FK_I_CURSOR, "ICON_FK_I_CURSOR" },
    { ICON_FK_OBJECT_GROUP, "ICON_FK_OBJECT_GROUP" },
    { ICON_FK_OBJECT_UNGROUP, "ICON_FK_OBJECT_UNGROUP" },
    { ICON_FK_STICKY_NOTE, "ICON_FK_STICKY_NOTE" },
    { ICON_FK_STICKY_NOTE_O, "ICON_FK_STICKY_NOTE_O" },
    { ICON_FK_CC_JCB, "ICON_FK_CC_JCB" },
    { ICON_FK_CC_DINERS_CLUB, "ICON_FK_CC_DINERS_CLUB" },
    { ICON_FK_CLONE, "ICON_FK_CLONE" },
    { ICON_FK_BALANCE_SCALE, "ICON_FK_BALANCE_SCALE" },
    { ICON_FK_HOURGLASS_O, "ICON_FK_HOURGLASS_O" },
    { ICON_FK_HOURGLASS_START, "ICON_FK_HOURGLASS_START" },
    { ICON_FK_HOURGLASS_HALF, "ICON_FK_HOURGLASS_HALF" },
    { ICON_FK_HOURGLASS_END, "ICON_FK_HOURGLASS_END" },
    { ICON_FK_HOURGLASS, "ICON_FK_HOURGLASS" },
    { ICON_FK_HAND_ROCK_O, "ICON_FK_HAND_ROCK_O" },
    { ICON_FK_HAND_PAPER_O, "ICON_FK_HAND_PAPER_O" },
    { ICON_FK_HAND_SCISSORS_O, "ICON_FK_HAND_SCISSORS_O" },
    { ICON_FK_HAND_LIZARD_O, "ICON_FK_HAND_LIZARD_O" },
    { ICON_FK_HAND_SPOCK_O, "ICON_FK_HAND_SPOCK_O" },
    { ICON_FK_HAND_POINTER_O, "ICON_FK_HAND_POINTER_O" },
    { ICON_FK_HAND_PEACE_O, "ICON_FK_HAND_PEACE_O" },
    { ICON_FK_TRADEMARK, "ICON_FK_TRADEMARK" },
    { ICON_FK_REGISTERED, "ICON_FK_REGISTERED" },
    { ICON_FK_CREATIVE_COMMONS, "ICON_FK_CREATIVE_COMMONS" },
    { ICON_FK_GG, "ICON_FK_GG" },
    { ICON_FK_GG_CIRCLE, "ICON_FK_GG_CIRCLE" },
    { ICON_FK_TRIPADVISOR, "ICON_FK_TRIPADVISOR" },
    { ICON_FK_ODNOKLASSNIKI, "ICON_FK_ODNOKLASSNIKI" },
    { ICON_FK_ODNOKLASSNIKI_SQUARE, "ICON_FK_ODNOKLASSNIKI_SQUARE" },
    { ICON_FK_GET_POCKET, "ICON_FK_GET_POCKET" },
    { ICON_FK_WIKIPEDIA_W, "ICON_FK_WIKIPEDIA_W" },
    { ICON_FK_SAFARI, "ICON_FK_SAFARI" },
    { ICON_FK_CHROME, "ICON_FK_CHROME" },
    { ICON_FK_FIREFOX, "ICON_FK_FIREFOX" },
    { ICON_FK_OPERA, "ICON_FK_OPERA" },
    { ICON_FK_INTERNET_EXPLORER, "ICON_FK_INTERNET_EXPLORER" },
    { ICON_FK_TELEVISION, "ICON_FK_TELEVISION" },
    { ICON_FK_CONTAO, "ICON_FK_CONTAO" },
    { ICON_FK_500PX, "ICON_FK_500PX" },
    { ICON_FK_AMAZON, "ICON_FK_AMAZON" },
    { ICON_FK_CALENDAR_PLUS_O, "ICON_FK_CALENDAR_PLUS_O" },
    { ICON_FK_CALENDAR_MINUS_O, "ICON_FK_CALENDAR_MINUS_O" },
    { ICON_FK_CALENDAR_TIMES_O, "ICON_FK_CALENDAR_TIMES_O" },
    { ICON_FK_CALENDAR_CHECK_O, "ICON_FK_CALENDAR_CHECK_O" },
    { ICON_FK_INDUSTRY, "ICON_FK_INDUSTRY" },
    { ICON_FK_MAP_PIN, "ICON_FK_MAP_PIN" },
    { ICON_FK_MAP_SIGNS, "ICON_FK_MAP_SIGNS" },
    { ICON_FK_MAP_O, "ICON_FK_MAP_O" },
    { ICON_FK_MAP, "ICON_FK_MAP" },
    { ICON_FK_COMMENTING, "ICON_FK_COMMENTING" },
    { ICON_FK_COMMENTING_O, "ICON_FK_COMMENTING_O" },
    { ICON_FK_HOUZZ, "ICON_FK_HOUZZ" },
    { ICON_FK_VIMEO, "ICON_FK_VIMEO" },
    { ICON_FK_BLACK_TIE, "ICON_FK_BLACK_TIE" },
    { ICON_FK_FONTICONS, "ICON_FK_FONTICONS" },
    { ICON_FK_REDDIT_ALIEN, "ICON_FK_REDDIT_ALIEN" },
    { ICON_FK_EDGE, "ICON_FK_EDGE" },
    { ICON_FK_CREDIT_CARD_ALT, "ICON_FK_CREDIT_CARD_ALT" },
    { ICON_FK_CODIEPIE, "ICON_FK_CODIEPIE" },
    { ICON_FK_MODX, "ICON_FK_MODX" },
    { ICON_FK_FORT_AWESOME, "ICON_FK_FORT_AWESOME" },
    { ICON_FK_USB, "ICON_FK_USB" },
    { ICON_FK_PRODUCT_HUNT, "ICON_FK_PRODUCT_HUNT" },
    { ICON_FK_MIXCLOUD, "ICON_FK_MIXCLOUD" },
    { ICON_FK_SCRIBD, "ICON_FK_SCRIBD" },
    { ICON_FK_PAUSE_CIRCLE, "ICON_FK_PAUSE_CIRCLE" },
    { ICON_FK_PAUSE_CIRCLE_O, "ICON_FK_PAUSE_CIRCLE_O" },
    { ICON_FK_STOP_CIRCLE, "ICON_FK_STOP_CIRCLE" },
    { ICON_FK_STOP_CIRCLE_O, "ICON_FK_STOP_CIRCLE_O" },
    { ICON_FK_SHOPPING_BAG, "ICON_FK_SHOPPING_BAG" },
    { ICON_FK_SHOPPING_BASKET, "ICON_FK_SHOPPING_BASKET" },
    { ICON_FK_HASHTAG, "ICON_FK_HASHTAG" },
    { ICON_FK_BLUETOOTH, "ICON_FK_BLUETOOTH" },
    { ICON_FK_BLUETOOTH_B, "ICON_FK_BLUETOOTH_B" },
    { ICON_FK_PERCENT, "ICON_FK_PERCENT" },
    { ICON_FK_GITLAB, "ICON_FK_GITLAB" },
    { ICON_FK_WPBEGINNER, "ICON_FK_WPBEGINNER" },
    { ICON_FK_WPFORMS, "ICON_FK_WPFORMS" },
    { ICON_FK_ENVIRA, "ICON_FK_ENVIRA" },
    { ICON_FK_UNIVERSAL_ACCESS, "ICON_FK_UNIVERSAL_ACCESS" },
    { ICON_FK_WHEELCHAIR_ALT, "ICON_FK_WHEELCHAIR_ALT" },
    { ICON_FK_QUESTION_CIRCLE_O, "ICON_FK_QUESTION_CIRCLE_O" },
    { ICON_FK_BLIND, "ICON_FK_BLIND" },
    { ICON_FK_AUDIO_DESCRIPTION, "ICON_FK_AUDIO_DESCRIPTION" },
    { ICON_FK_VOLUME_CONTROL_PHONE, "ICON_FK_VOLUME_CONTROL_PHONE" },
    { ICON_FK_BRAILLE, "ICON_FK_BRAILLE" },
    { ICON_FK_ASSISTIVE_LISTENING_SYSTEMS, "ICON_FK_ASSISTIVE_LISTENING_SYSTEMS" },
    { ICON_FK_AMERICAN_SIGN_LANGUAGE_INTERPRETING, "ICON_FK_AMERICAN_SIGN_LANGUAGE_INTERPRETING" },
    { ICON_FK_DEAF, "ICON_FK_DEAF" },
    { ICON_FK_GLIDE, "ICON_FK_GLIDE" },
    { ICON_FK_GLIDE_G, "ICON_FK_GLIDE_G" },
    { ICON_FK_SIGN_LANGUAGE, "ICON_FK_SIGN_LANGUAGE" },
    { ICON_FK_LOW_VISION, "ICON_FK_LOW_VISION" },
    { ICON_FK_VIADEO, "ICON_FK_VIADEO" },
    { ICON_FK_VIADEO_SQUARE, "ICON_FK_VIADEO_SQUARE" },
    { ICON_FK_SNAPCHAT, "ICON_FK_SNAPCHAT" },
    { ICON_FK_SNAPCHAT_GHOST, "ICON_FK_SNAPCHAT_GHOST" },
    { ICON_FK_SNAPCHAT_SQUARE, "ICON_FK_SNAPCHAT_SQUARE" },
    { ICON_FK_FIRST_ORDER, "ICON_FK_FIRST_ORDER" },
    { ICON_FK_YOAST, "ICON_FK_YOAST" },
    { ICON_FK_THEMEISLE, "ICON_FK_THEMEISLE" },
    { ICON_FK_GOOGLE_PLUS_OFFICIAL, "ICON_FK_GOOGLE_PLUS_OFFICIAL" },
    { ICON_FK_FONT_AWESOME, "ICON_FK_FONT_AWESOME" },
    { ICON_FK_HANDSHAKE_O, "ICON_FK_HANDSHAKE_O" },
    { ICON_FK_ENVELOPE_OPEN, "ICON_FK_ENVELOPE_OPEN" },
    { ICON_FK_ENVELOPE_OPEN_O, "ICON_FK_ENVELOPE_OPEN_O" },
    { ICON_FK_LINODE, "ICON_FK_LINODE" },
    { ICON_FK_ADDRESS_BOOK, "ICON_FK_ADDRESS_BOOK" },
    { ICON_FK_ADDRESS_BOOK_O, "ICON_FK_ADDRESS_BOOK_O" },
    { ICON_FK_ADDRESS_CARD, "ICON_FK_ADDRESS_CARD" },
    { ICON_FK_ADDRESS_CARD_O, "ICON_FK_ADDRESS_CARD_O" },
    { ICON_FK_USER_CIRCLE, "ICON_FK_USER_CIRCLE" },
    { ICON_FK_USER_CIRCLE_O, "ICON_FK_USER_CIRCLE_O" },
    { ICON_FK_USER_O, "ICON_FK_USER_O" },
    { ICON_FK_ID_BADGE, "ICON_FK_ID_BADGE" },
    { ICON_FK_ID_CARD, "ICON_FK_ID_CARD" },
    { ICON_FK_ID_CARD_O, "ICON_FK_ID_CARD_O" },
    { ICON_FK_QUORA, "ICON_FK_QUORA" },
    { ICON_FK_FREE_CODE_CAMP, "ICON_FK_FREE_CODE_CAMP" },
    { ICON_FK_TELEGRAM, "ICON_FK_TELEGRAM" },
    { ICON_FK_THERMOMETER_FULL, "ICON_FK_THERMOMETER_FULL" },
    { ICON_FK_THERMOMETER_THREE_QUARTERS, "ICON_FK_THERMOMETER_THREE_QUARTERS" },
    { ICON_FK_THERMOMETER_HALF, "ICON_FK_THERMOMETER_HALF" },
    { ICON_FK_THERMOMETER_QUARTER, "ICON_FK_THERMOMETER_QUARTER" },
    { ICON_FK_THERMOMETER_EMPTY, "ICON_FK_THERMOMETER_EMPTY" },
    { ICON_FK_SHOWER, "ICON_FK_SHOWER" },
    { ICON_FK_BATH, "ICON_FK_BATH" },
    { ICON_FK_PODCAST, "ICON_FK_PODCAST" },
    { ICON_FK_WINDOW_MAXIMIZE, "ICON_FK_WINDOW_MAXIMIZE" },
    { ICON_FK_WINDOW_MINIMIZE, "ICON_FK_WINDOW_MINIMIZE" },
    { ICON_FK_WINDOW_RESTORE, "ICON_FK_WINDOW_RESTORE" },
    { ICON_FK_WINDOW_CLOSE, "ICON_FK_WINDOW_CLOSE" },
    { ICON_FK_WINDOW_CLOSE_O, "ICON_FK_WINDOW_CLOSE_O" },
    { ICON_FK_BANDCAMP, "ICON_FK_BANDCAMP" },
    { ICON_FK_GRAV, "ICON_FK_GRAV" },
    { ICON_FK_ETSY, "ICON_FK_ETSY" },
    { ICON_FK_IMDB, "ICON_FK_IMDB" },
    { ICON_FK_RAVELRY, "ICON_FK_RAVELRY" },
    { ICON_FK_EERCAST, "ICON_FK_EERCAST" },
    { ICON_FK_MICROCHIP, "ICON_FK_MICROCHIP" },
    { ICON_FK_SNOWFLAKE_O, "ICON_FK_SNOWFLAKE_O" },
    { ICON_FK_SUPERPOWERS, "ICON_FK_SUPERPOWERS" },
    { ICON_FK_WPEXPLORER, "ICON_FK_WPEXPLORER" },
    { ICON_FK_MEETUP, "ICON_FK_MEETUP" },
    { ICON_FK_MASTODON, "ICON_FK_MASTODON" },
    { ICON_FK_MASTODON_ALT, "ICON_FK_MASTODON_ALT" },
    { ICON_FK_FORK_AWESOME, "ICON_FK_FORK_AWESOME" },
    { ICON_FK_PEERTUBE, "ICON_FK_PEERTUBE" },
    { ICON_FK_DIASPORA, "ICON_FK_DIASPORA" },
    { ICON_FK_FRIENDICA, "ICON_FK_FRIENDICA" },
    { ICON_FK_GNU_SOCIAL, "ICON_FK_GNU_SOCIAL" },
    { ICON_FK_LIBERAPAY_SQUARE, "ICON_FK_LIBERAPAY_SQUARE" },
    { ICON_FK_LIBERAPAY, "ICON_FK_LIBERAPAY" },
    { ICON_FK_SCUTTLEBUTT, "ICON_FK_SCUTTLEBUTT" },
    { ICON_FK_HUBZILLA, "ICON_FK_HUBZILLA" },
    { ICON_FK_SOCIAL_HOME, "ICON_FK_SOCIAL_HOME" },
    { ICON_FK_ARTSTATION, "ICON_FK_ARTSTATION" },
    { ICON_FK_DISCORD, "ICON_FK_DISCORD" },
    { ICON_FK_DISCORD_ALT, "ICON_FK_DISCORD_ALT" },
    { ICON_FK_PATREON, "ICON_FK_PATREON" },
    { ICON_FK_SNOWDRIFT, "ICON_FK_SNOWDRIFT" },
    { ICON_FK_ACTIVITYPUB, "ICON_FK_ACTIVITYPUB" },
    { ICON_FK_ETHEREUM, "ICON_FK_ETHEREUM" },
    { ICON_FK_KEYBASE, "ICON_FK_KEYBASE" },
    { ICON_FK_SHAARLI, "ICON_FK_SHAARLI" },
    { ICON_FK_SHAARLI_O, "ICON_FK_SHAARLI_O" },
    { ICON_FK_KEY_MODERN, "ICON_FK_KEY_MODERN" },
    { ICON_FK_XMPP, "ICON_FK_XMPP" },
    { ICON_FK_ARCHIVE_ORG, "ICON_FK_ARCHIVE_ORG" },
    { ICON_FK_FREEDOMBOX, "ICON_FK_FREEDOMBOX" },
    { ICON_FK_FACEBOOK_MESSENGER, "ICON_FK_FACEBOOK_MESSENGER" },
    { ICON_FK_DEBIAN, "ICON_FK_DEBIAN" },
    { ICON_FK_MASTODON_SQUARE, "ICON_FK_MASTODON_SQUARE" },
    { ICON_FK_TIPEEE, "ICON_FK_TIPEEE" },
    { ICON_FK_REACT, "ICON_FK_REACT" },
    { ICON_FK_DOGMAZIC, "ICON_FK_DOGMAZIC" },
    { ICON_FK_ZOTERO, "ICON_FK_ZOTERO" },
    { ICON_FK_NODEJS, "ICON_FK_NODEJS" },
    { ICON_FK_NEXTCLOUD, "ICON_FK_NEXTCLOUD" },
    { ICON_FK_NEXTCLOUD_SQUARE, "ICON_FK_NEXTCLOUD_SQUARE" },
    { ICON_FK_HACKADAY, "ICON_FK_HACKADAY" },
    { ICON_FK_LARAVEL, "ICON_FK_LARAVEL" },
    { ICON_FK_SIGNALAPP, "ICON_FK_SIGNALAPP" },
    { ICON_FK_GNUPG, "ICON_FK_GNUPG" },
    { ICON_FK_PHP, "ICON_FK_PHP" },
    { ICON_FK_FFMPEG, "ICON_FK_FFMPEG" },
    { ICON_FK_JOPLIN, "ICON_FK_JOPLIN" },
    { ICON_FK_SYNCTHING, "ICON_FK_SYNCTHING" },
    { ICON_FK_INKSCAPE, "ICON_FK_INKSCAPE" },
    { ICON_FK_MATRIX_ORG, "ICON_FK_MATRIX_ORG" },
    { ICON_FK_PIXELFED, "ICON_FK_PIXELFED" },
    { ICON_FK_BOOTSTRAP, "ICON_FK_BOOTSTRAP" },
    { ICON_FK_DEV_TO, "ICON_FK_DEV_TO" },
    { ICON_FK_HASHNODE, "ICON_FK_HASHNODE" },
    { ICON_FK_JIRAFEAU, "ICON_FK_JIRAFEAU" },
    { ICON_FK_EMBY, "ICON_FK_EMBY" },
    { ICON_FK_WIKIDATA, "ICON_FK_WIKIDATA" },
    { ICON_FK_GIMP, "ICON_FK_GIMP" },
    { ICON_FK_C, "ICON_FK_C" },
    { ICON_FK_DIGITALOCEAN, "ICON_FK_DIGITALOCEAN" },
    { ICON_FK_ATT, "ICON_FK_ATT" },
    { ICON_FK_GITEA, "ICON_FK_GITEA" },
    { ICON_FK_FILE_EPUB, "ICON_FK_FILE_EPUB" },
    { ICON_FK_PYTHON, "ICON_FK_PYTHON" },
    { ICON_FK_ARCHLINUX, "ICON_FK_ARCHLINUX" },
    { ICON_FK_PLEROMA, "ICON_FK_PLEROMA" },
    { ICON_FK_UNSPLASH, "ICON_FK_UNSPLASH" },
    { ICON_FK_HACKSTER, "ICON_FK_HACKSTER" },
    { ICON_FK_SPELL_CHECK, "ICON_FK_SPELL_CHECK" },
    { ICON_FK_MOON, "ICON_FK_MOON" },
    { ICON_FK_SUN, "ICON_FK_SUN" },
    { ICON_FK_F_DROID, "ICON_FK_F_DROID" },
    { ICON_FK_BIOMETRIC, "ICON_FK_BIOMETRIC" },
    { ICON_FK_WIRE, "ICON_FK_WIRE" },
    { ICON_FK_TOR_ONION, "ICON_FK_TOR_ONION" },
    { ICON_FK_VOLUME_MUTE, "ICON_FK_VOLUME_MUTE" },
    { ICON_FK_BELL_RINGING, "ICON_FK_BELL_RINGING" },
    { ICON_FK_BELL_RINGING_O, "ICON_FK_BELL_RINGING_O" },
    { ICON_FK_HAL, "ICON_FK_HAL" },
    { ICON_FK_JUPYTER, "ICON_FK_JUPYTER" },
    { ICON_FK_JULIA, "ICON_FK_JULIA" },
    { ICON_FK_CLASSICPRESS, "ICON_FK_CLASSICPRESS" },
    { ICON_FK_CLASSICPRESS_CIRCLE, "ICON_FK_CLASSICPRESS_CIRCLE" },
    { ICON_FK_OPEN_COLLECTIVE, "ICON_FK_OPEN_COLLECTIVE" },
    { ICON_FK_ORCID, "ICON_FK_ORCID" },
    { ICON_FK_RESEARCHGATE, "ICON_FK_RESEARCHGATE" },
    { ICON_FK_FUNKWHALE, "ICON_FK_FUNKWHALE" },
    { ICON_FK_ASKFM, "ICON_FK_ASKFM" },
    { ICON_FK_BLOCKSTACK, "ICON_FK_BLOCKSTACK" },
    { ICON_FK_BOARDGAMEGEEK, "ICON_FK_BOARDGAMEGEEK" },
    { ICON_FK_BUNNY, "ICON_FK_BUNNY" },
    { ICON_FK_BUYMEACOFFEE, "ICON_FK_BUYMEACOFFEE" },
    { ICON_FK_CC_BY, "ICON_FK_CC_BY" },
    { ICON_FK_CC_CC, "ICON_FK_CC_CC" },
    { ICON_FK_CC_NC_EU, "ICON_FK_CC_NC_EU" },
    { ICON_FK_CC_NC_JP, "ICON_FK_CC_NC_JP" },
    { ICON_FK_CC_NC, "ICON_FK_CC_NC" },
    { ICON_FK_CC_ND, "ICON_FK_CC_ND" },
    { ICON_FK_CC_PD, "ICON_FK_CC_PD" },
    { ICON_FK_CC_REMIX, "ICON_FK_CC_REMIX" },
    { ICON_FK_CC_SA, "ICON_FK_CC_SA" },
    { ICON_FK_CC_SHARE, "ICON_FK_CC_SHARE" },
    { ICON_FK_CC_ZERO, "ICON_FK_CC_ZERO" },
    { ICON_FK_CONWAY_GLIDER, "ICON_FK_CONWAY_GLIDER" },
    { ICON_FK_CSHARP, "ICON_FK_CSHARP" },
    { ICON_FK_EMAIL_BULK, "ICON_FK_EMAIL_BULK" },
    { ICON_FK_EMAIL_BULK_O, "ICON_FK_EMAIL_BULK_O" },
    { ICON_FK_GNU, "ICON_FK_GNU" },
    { ICON_FK_GOOGLE_PLAY, "ICON_FK_GOOGLE_PLAY" },
    { ICON_FK_HEROKU, "ICON_FK_HEROKU" },
    { ICON_FK_HOME_ASSISTANT, "ICON_FK_HOME_ASSISTANT" },
    { ICON_FK_JAVA, "ICON_FK_JAVA" },
    { ICON_FK_MARIADB, "ICON_FK_MARIADB" },
    { ICON_FK_MARKDOWN, "ICON_FK_MARKDOWN" },
    { ICON_FK_MYSQL, "ICON_FK_MYSQL" },
    { ICON_FK_NORDCAST, "ICON_FK_NORDCAST" },
    { ICON_FK_PLUME, "ICON_FK_PLUME" },
    { ICON_FK_POSTGRESQL, "ICON_FK_POSTGRESQL" },
    { ICON_FK_SASS_ALT, "ICON_FK_SASS_ALT" },
    { ICON_FK_SASS, "ICON_FK_SASS" },
    { ICON_FK_SKATE, "ICON_FK_SKATE" },
    { ICON_FK_SKETCHFAB, "ICON_FK_SKETCHFAB" },
    { ICON_FK_TEX, "ICON_FK_TEX" },
    { ICON_FK_TEXTPATTERN, "ICON_FK_TEXTPATTERN" },
    { ICON_FK_UNITY, "ICON_FK_UNITY" },
    { ICON_FK_HEDGEDOC, "ICON_FK_HEDGEDOC" },
    { ICON_FK_FEDIVERSE, "ICON_FK_FEDIVERSE" },
    { ICON_FK_PROFTPD, "ICON_FK_PROFTPD" },
    { ICON_FK_OSI, "ICON_FK_OSI" },
    { ICON_FK_EYEEM, "ICON_FK_EYEEM" },
    { ICON_FK_EYEEM_O, "ICON_FK_EYEEM_O" },
    { ICON_FK_CODEBERG, "ICON_FK_CODEBERG" },
    { ICON_FK_DISCOURSE, "ICON_FK_DISCOURSE" },
    { ICON_FK_MUMBLE, "ICON_FK_MUMBLE" },
    { ICON_FK_FREEDESKTOP, "ICON_FK_FREEDESKTOP" },
    { ICON_FK_JAVASCRIPT, "ICON_FK_JAVASCRIPT" },
    { ICON_FK_LEMMY, "ICON_FK_LEMMY" },
    { ICON_FK_IPFS, "ICON_FK_IPFS" },
    { ICON_FK_CANONICAL, "ICON_FK_CANONICAL" },
    { ICON_FK_UBUNTU, "ICON_FK_UBUNTU" },
};

void GuiWinExperiment::_DrawIcons()
{
    const int max = ImGui::GetContentRegionAvail().x / (GUI_VAR.iconSize.x + GUI_VAR.imguiStyle->ItemSpacing.x);
    int num = 0;
    for (auto& e : ICONS) {
        if (ImGui::Button(e.icon, GUI_VAR.iconSize)) {
            ImGui::SetClipboardText(e.name);
        }
        Gui::ItemTooltip(e.name);
        num++;
        if (num < max) {
            ImGui::SameLine();
        } else {
            num = 0;
        }
    }
}

/* ****************************************************************************************************************** */
}  // namespace ffgui

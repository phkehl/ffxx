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
#include <chrono>
//
#include <fpsdk_common/thread.hpp>
using namespace fpsdk::common::thread;
//
#include "gui_win_play.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

GuiWinPlay::GuiWinPlay() /* clang-format off */ :
    GuiWin("Play", { 100.0, 50.0, 0.0, 0.0 }, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDocking),
    tetrisGame_   { tg_create(TETRIS_ROWS, TERIS_COLS) },
    tetrisMove_   { TM_NONE }  // clang-format on
{
}

GuiWinPlay::~GuiWinPlay()
{
    if (tetrisThread_) {
        tetrisThreadAbort_ = true;
        tetrisThread_->join();
    }

    if (tetrisGame_) {
        tg_delete(tetrisGame_);
        tetrisGame_ = nullptr;
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinPlay::DrawWindow()
{
    if (!_DrawWindowBegin()) {
        return;
    }

    if (ImGui::BeginTabBar("##Tabs", ImGuiTabBarFlags_FittingPolicyScroll)) {
        if (ImGui::BeginTabItem("Tetris")) {
            _DrawTetris();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    playWinFocused_ = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);

    _DrawWindowEnd();
}

// ---------------------------------------------------------------------------------------------------------------------

#define USE_TM_HOLD \
    0  // doesn't quiet work (it can get stuck in some loop sometimes), https://github.com/brenns10/tetris/issues/9

void GuiWinPlay::_DrawTetris()
{
    std::lock_guard<std::mutex> lock(tetrisGameMutex_);

    const float lineHeight = GUI_VAR.charSizeY + GUI_VAR.imguiStyle->ItemSpacing.y;
    const float cellSize = 2 * GUI_VAR.charSizeX;
    const Vec2f cell{ cellSize, cellSize };
    const float padding = cellSize;
    const Vec2f boardSize{ cellSize * TERIS_COLS, cellSize * TETRIS_ROWS };
    const Vec2f nextSize{ cellSize * 4, cellSize * 4 };
    const Vec2f canvasSize{ padding + boardSize.x + padding + nextSize.x + padding, padding + boardSize.y + padding };

    if (!ImGui::BeginChild("Board", canvasSize)) {
        ImGui::EndChild();
        return;
    }

    ImDrawList* draw = ImGui::GetWindowDrawList();

    // Canvas
    const Vec2f canvas0 = ImGui::GetCursorScreenPos();
    const Vec2f canvasS = ImGui::GetContentRegionAvail();
    const Vec2f canvas1 = canvas0 + canvasS;
    draw->AddRect(canvas0, canvas1, C_C_GREY());

    // Board
    const Vec2f board0 = canvas0 + Vec2f(padding, padding);
    const Vec2f board1 = board0 + boardSize;
    draw->AddRect(board0 - Vec2f(2, 2), board1 + Vec2f(2, 2), C_C_WHITE());

    // Next block
    const Vec2f next0{ board1.x + padding, board0.y + lineHeight };
    const Vec2f next1 = next0 + nextSize;
    draw->AddRect(next0 - Vec2f(2, 2), next1 + Vec2f(2, 2), C_C_WHITE());
    ImGui::SetCursorScreenPos(ImVec2(next0.x, next0.y - lineHeight));
    ImGui::TextUnformatted("Next:");

    // Stored block
    const Vec2f stored0{ board1.x + padding, next1.y + padding + lineHeight };
    const Vec2f stored1 = stored0 + nextSize;
#if USE_TM_HOLD
    draw->AddRect(stored0 - Vec2f(2, 2), stored1 + Vec2f(2, 2), C_WHITE());
    ImGui::SetCursorScreenPos(Vec2f(stored0.x, stored0.y - lineHeight));
    ImGui::TextUnformatted("Stored:");
#endif

    // Score
    float y0 = stored1.y + padding;
    const Vec2f score0{ board1.x + padding, y0 };
    const Vec2f score1{ score0.x, score0.y + lineHeight };
    ImGui::SetCursorScreenPos(score0);
    ImGui::TextUnformatted("Score:");
    ImGui::SetCursorScreenPos(score1);
    ImGui::Text("%d", tetrisGame_->points);
    y0 += padding + lineHeight + lineHeight;

    // Level
    const Vec2f level0{ board1.x + padding, y0 };
    const Vec2f level1{ level0.x, level0.y + lineHeight };
    ImGui::SetCursorScreenPos(level0);
    ImGui::TextUnformatted("Level:");
    ImGui::SetCursorScreenPos(level1);
    ImGui::Text("%d", tetrisGame_->level);
    y0 += padding + lineHeight + lineHeight;

    // Lines
    const Vec2f lines0{ board1.x + padding, y0 };
    const Vec2f lines1{ lines0.x, lines0.y + lineHeight };
    ImGui::SetCursorScreenPos(lines0);
    ImGui::TextUnformatted("Lines:");
    ImGui::SetCursorScreenPos(lines1);
    ImGui::Text("%d", tetrisGame_->lines_remaining);
    y0 += padding + lineHeight + lineHeight;

    // Draw game
    if (tetrisGame_->falling.loc.row > 0) {
        _DrawTetromino(draw, next0, cell, tetrisGame_->next);
        _DrawTetromino(draw, stored0, cell, tetrisGame_->stored);
    }
    for (int row = 0; row < tetrisGame_->rows; row++) {
        for (int col = 0; col < tetrisGame_->cols; col++) {
            const tetris_cell c = (tetris_cell)tg_get(tetrisGame_, row, col);
            if (c != TC_EMPTY) {
                const Vec2f block0{ board0.x + (col * cell.x), board0.y + (row * cell.y) };
                const tetris_type type = (tetris_type)(c - 1);
                draw->AddRectFilled(block0, block0 + cell, _TetrominoColour(type));
                draw->AddRect(block0, block0 + cell, _TetrominoColour(type, true));
            }
        }
    }

    // Handle keys
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
        tetrisMove_ = TM_CLOCK;
    } else if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) {
        tetrisMove_ = TM_LEFT;
    } else if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) {
        tetrisMove_ = TM_RIGHT;
    } else if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
        tetrisMove_ = (tetris_move)(100 + 20);
    } else if (ImGui::IsKeyPressed(ImGuiKey_Enter)) {
        tetrisMove_ = TM_DROP;
    } else if (ImGui::IsKeyPressed(ImGuiKey_Space)) {
        tetrisThreadPaused_ = !tetrisThreadPaused_;
    }
#if USE_TM_HOLD
    else if (ImGui::IsKeyPressed(ImGuiKey_Space)) {
        tetrisMove_ = TM_HOLD;
    }
#endif

    // Game finished
    if (!tetrisThreadRunning_ && tetrisThread_) {
        tetrisThread_->join();
        tetrisThread_ = nullptr;
    }

    // Draw start button
    if (!tetrisThread_) {
        draw->AddRectFilled(board0, board1, C_C_GREY() & IM_COL32(0xff, 0xff, 0xff, 0x7f));
        ImGui::SetCursorScreenPos(board0 + ((boardSize - GUI_VAR.iconSize) * 0.5));
        if (ImGui::Button(ICON_FK_PLAY "##Play", GUI_VAR.iconSize)) {
            tg_init(tetrisGame_, TETRIS_ROWS, TERIS_COLS);
            tetrisThreadAbort_ = false;
            tetrisThreadPaused_ = false;
            tetrisThread_ = std::make_unique<std::thread>(&GuiWinPlay::_TetrisThread, this);
        }
        Gui::ItemTooltip("Play");
    }

    ImGui::EndChild();
}

// ---------------------------------------------------------------------------------------------------------------------

ImU32 GuiWinPlay::_TetrominoColour(const tetris_type type, const bool dim)
{
    // https://en.wikipedia.org/wiki/Tetromino
    ImU32 colour = C_C_GREY();
    switch (type) {  // clang-format off
        case TET_I: colour = C_C_CYAN();       break;
        case TET_J: colour = C_C_BRIGHTBLUE(); break;
        case TET_L: colour = C_C_ORANGE();     break;
        case TET_O: colour = C_C_YELLOW();     break;
        case TET_S: colour = C_C_GREEN();      break;
        case TET_T: colour = C_C_MAGENTA();    break;
        case TET_Z: colour = C_C_RED();        break;
    }  // clang-format on
    if (dim) {
        return colour & IM_COL32(0x7f, 0x7f, 0x7f, 0x7f);
    } else {
        return colour;
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinPlay::_DrawTetromino(ImDrawList* draw, const Vec2f& p0, const Vec2f& cell, const tetris_block& block)
{
    if (block.typ >= 0) {
        Vec2f offs{ 0, 0 };
        switch (block.typ) {  // clang-format off
            case TET_I: offs.y = cell.y / 2; break;
            case TET_J:
            case TET_L: offs.x = cell.x / 2; offs.y = cell.y; break;
            case TET_O: offs.y = cell.y; break;
            case TET_S:
            case TET_T:
            case TET_Z: offs.x = cell.x / 2; offs.y = cell.y; break;
        }  // clang-format on
        for (int b = 0; b < TETRIS; b++) {
            const tetris_location c = TETROMINOS[block.typ][block.ori][b];
            const Vec2f block0 = p0 + offs + Vec2f(cell.x * c.col, cell.y * c.row);
            draw->AddRectFilled(block0, block0 + cell, _TetrominoColour((tetris_type)block.typ));
            draw->AddRect(block0, block0 + cell, _TetrominoColour((tetris_type)block.typ, true));
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void GuiWinPlay::_TetrisThread()
{
    SetThreadName("tetris");
    tetrisThreadRunning_ = true;
    while (!tetrisThreadAbort_ && tetrisThreadRunning_) {
        if (WinIsOpen() && playWinFocused_ && !tetrisThreadPaused_) {
            std::lock_guard<std::mutex> lock(tetrisGameMutex_);

            if (tetrisMove_ >= 100) {
                int n = tetrisMove_ - 100;
                while (tetrisThreadRunning_ && (n > 0)) {
                    tetrisThreadRunning_ = tg_tick(tetrisGame_, TM_NONE);
                    n--;
                }
            } else {
                tetrisThreadRunning_ = tg_tick(tetrisGame_, tetrisMove_);
            }
        }

        tetrisMove_ = TM_NONE;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    tetrisThreadRunning_ = false;
}

/* ****************************************************************************************************************** */
}  // namespace ffgui

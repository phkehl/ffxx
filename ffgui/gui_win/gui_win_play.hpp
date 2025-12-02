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

#ifndef __GUI_WIN_PLAY_HPP__
#define __GUI_WIN_PLAY_HPP__

//
#include "ffgui_inc.hpp"
//
#include <atomic>
#include <mutex>
#include <thread>
//
extern "C" {
#include "tetris.h"
}
//
#include "gui_win.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

class GuiWinPlay : public GuiWin
{
   public:
    GuiWinPlay();
    ~GuiWinPlay();

    void DrawWindow() override final;

   private:
    std::atomic<bool> playWinFocused_;

    static constexpr int TETRIS_ROWS = 22;
    static constexpr int TERIS_COLS = 10;
    tetris_game* tetrisGame_;
    std::mutex tetrisGameMutex_;
    std::atomic<tetris_move> tetrisMove_;
    std::unique_ptr<std::thread> tetrisThread_;
    std::atomic<bool> tetrisThreadAbort_;
    std::atomic<bool> tetrisThreadRunning_;
    std::atomic<bool> tetrisThreadPaused_;

    void _TetrisThread();

    void _DrawTetris();
    void _DrawTetromino(ImDrawList* draw, const Vec2f& p0, const Vec2f& square, const tetris_block& block);
    ImU32 _TetrominoColour(const tetris_type type, const bool dim = false);
};

/* ****************************************************************************************************************** */
}  // namespace ffgui
#endif  // __GUI_WIN_PLAY_HPP__

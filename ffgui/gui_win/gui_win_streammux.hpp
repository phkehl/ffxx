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

#ifndef __GUI_WIN_STREAMMUX_HPP__
#define __GUI_WIN_STREAMMUX_HPP__

//
#include "ffgui_inc.hpp"
//
#include <atomic>
#include <future>
//
#include <boost/asio.hpp>
#include <boost/process.hpp>
//
#include "gui_widget_log.hpp"
#include "gui_widget_tabbar.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

class GuiWinStreammuxHelp : public GuiWin
{
   public:
    GuiWinStreammuxHelp(const std::string& name);
    ~GuiWinStreammuxHelp();

    void Loop(const Time& now) final;
    void DrawWindow() final;

    void Update(const std::string& streammuxExe);

   private:
    std::vector<std::string> help_;
    std::future<std::vector<std::string>> fut_;
    std::atomic<bool> ready_ = false;
    std::vector<std::string> _GetHelp(const std::string& streammuxExe);
};

// ---------------------------------------------------------------------------------------------------------------------

class GuiWinStreammux : public GuiWin
{
   public:
    GuiWinStreammux(const std::string& name);
    ~GuiWinStreammux();

    void Loop(const Time& now) final;
    void DrawWindow() final;

   private:

    struct SmCfg
    {
        std::string note;
        std::string exe;
        std::string args;

        inline bool operator<(const SmCfg& rhs) const { return note < rhs.note; }
        bool operator==(const SmCfg& other) const { return (note == other.note) && (exe == other.exe) && (args == other.args); }
        bool operator!=(const SmCfg& other) const { return !(*this == other); }
    };
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(SmCfg, note, exe, args)

    struct Config
    {
        SmCfg sm;
    };
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Config, sm)
    Config cfg_;

    struct SavedConfig
    {
        std::vector<SmCfg> sms;
    };
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(SavedConfig, sms)
    static SavedConfig savedCfg_;


    GuiWidgetTabbar tabbar_;
    GuiWidgetLog log_;
    bool dirty_ = false;
    bool cfgExeOk_ = false;
    bool cfgArgsOk_ = false;
    enum class DispState { CMD, OUT };
    DispState dispState_ = DispState::CMD;

    struct SmProc
    {
        SmProc(const std::vector<std::string>& argv);
        boost::asio::io_context ctx_;
        boost::process::async_pipe stdout_pipe_;
        boost::process::async_pipe stderr_pipe_;
        boost::asio::streambuf stdout_buf_;
        boost::asio::streambuf stderr_buf_;
        boost::process::child child_;
        int status_ = -1;
        bool sigintSent_ = false;
        void OnExit(const int status, const std::error_code& ec);
    };
    std::unique_ptr<SmProc> smProc_;
    void _DoRead(boost::process::async_pipe& pipe, boost::asio::streambuf& buf);
    void _OnRead(const boost::system::error_code& ec, std::size_t size, boost::process::async_pipe& pipe,
        boost::asio::streambuf& buf);

    std::vector<std::string> _MakeArgv(const SmCfg& cfg) const;

    void _Start();
    void _Stop();
    void _UpdateWinTitle();

    GuiWinStreammuxHelp smHelpWin_;
};

/* ****************************************************************************************************************** */
}  // namespace ffgui
#endif  // __GUI_WIN_STREAMMUX_HPP__

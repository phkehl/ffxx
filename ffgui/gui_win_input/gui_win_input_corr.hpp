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

#ifndef __GUI_WIN_CORR_HPP__
#define __GUI_WIN_CORR_HPP__

//
#include "ffgui_inc.hpp"
//
#include <fpsdk_common/parser/types.hpp>
using namespace fpsdk::common::parser;
//
#include "gui_widget_streamspec.hpp"
#include "gui_win_input.hpp"
#include "receiver.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

class GuiWinInputCorr : public GuiWinInput
{
   public:
    GuiWinInputCorr(const std::string& name);
    ~GuiWinInputCorr();

   private:

    enum class GgaSrc : int { OFF = 0, RECEIVER, MANUAL };
    enum class GgaTalker : int { GN = 0, GP };
    enum class CorrFwdCfg : int
    {
        Enable       = Bit<int>(0),
        Rtcm3Sta     = Bit<int>(1),
        Rtcm3Msm     = Bit<int>(2),
        Rtcm3Other   = Bit<int>(3),
        Spartn       = Bit<int>(4),
        Noise        = Bit<int>(5),
    };
    struct Config
    {
        GgaSrc ggaSrc = GgaSrc::RECEIVER;
        std::string ggaReceiver;
        float ggaInt = 5.0f;
        GgaTalker ggaTalker = GgaTalker::GN;
        double ggaHeight = 0.0;
        double ggaLon = 0.0;
        double ggaAlt = 0.0;
        int ggaNumSv = 10;
        bool ggaFix = true;
        std::map<std::string, CorrFwdCfg> corrFwd;

        static constexpr float GGAINT_MIN = 1.0f;
        static constexpr float GGAINT_MAX = 60.0f;
        static constexpr double GGALAT_MIN = -90.0;
        static constexpr double GGALAT_MAX = 90.0f;
        static constexpr double GGALON_MIN = -180.0;
        static constexpr double GGALON_MAX = 180.0f;
        static constexpr double GGAALT_MIN = -1000.0;
        static constexpr double GGAALT_MAX = 10000.0f;
    };
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Config, ggaSrc, ggaReceiver, ggaInt, ggaTalker, ggaHeight, ggaLon, ggaAlt, ggaNumSv, ggaFix, corrFwd)
    Config cfg_;

    void _ProcessData(const DataPtr& data) final;
    void _Loop(const Time& now) final;
    void _ClearData() final;

    void _DrawButtons() final;
    void _DrawStatus() final;
    void _DrawControls() final;

    GuiWidgetStreamSpec streamSpec_;
    bool triggerConnect_ = false;
    void _OnBaudrateChange(const uint32_t baudrate);

    struct Status
    {
        Status();
        void Update(const DataMsg& msg);
        void Update(const Time& now);

        bool okSta_ = false;
        bool okGps_ = false;
        bool okGal_ = false;
        bool okBds_ = false;
        bool okGlo_ = false;
        bool okBias_ = false;
        bool okNoise_ = false;

        std::size_t nSta_ = 0;
        std::size_t nGps_ = 0;
        std::size_t nGal_ = 0;
        std::size_t nBds_ = 0;
        std::size_t nGlo_ = 0;
        std::size_t nBias_ = 0;
        std::size_t nNoise_ = 0;

        double latency_ = 0.0;
        bool latencyOk_ = false;

       private:
        Time lastSta_;
        Time lastGps_;
        Time lastGal_;
        Time lastBds_;
        Time lastGlo_;
        Time lastBias_;
        Time lastNoise_;
        std::array<double, 10> latMeas_;
        std::size_t latIx_ = 0;

    };
    Status status_;

    void _UpdateGga();
    ParserMsg ggaMsg_;
    bool ggaMsgOk_ = false;
    Parser ggaParser_;
    std::string ggaInfo_;
    Time ggaLastSent_;
    Duration ggaInterval_;

#if 0
    std::mutex mutex_;

    // Receivers
    uint32_t guiDataSerial_;
    std::shared_ptr<InputReceiver> srcReceiver_;
    std::unordered_map<std::string, bool> dstReceivers_;
    GuiData::ReceiverList receivers_;

    // NTRIP client
    NtripClient ntripClient_;
    bool _SetCaster(const std::string& spec);

    // GGA input form
    std::string casterInput_;
    bool ggaAuto_;
    std::string ggaInt_;
    std::string ggaLat_;
    std::string ggaLon_;
    std::string ggaAlt_;
    std::string ggaNumSv_;
    bool ggaFix_;
    double ggaLastUpdate_;
    NtripClient::Pos ntripPos_;

    // Other GUI stuff
    GuiWidgetLog log_;

    // NTRIP thread
    fpsdk::common::thread::Thread thread_;
    bool Worker(fpsdk::common::thread::Thread& thread);
#endif
};

/* ****************************************************************************************************************** */
}  // namespace ffgui
#endif  // __GUI_WIN_CORR_HPP__

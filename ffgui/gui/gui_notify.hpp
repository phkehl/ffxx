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

#ifndef __GUI_WIDGET_NOTIFY_HPP__
#define __GUI_WIDGET_NOTIFY_HPP__

//
#include "ffgui_inc.hpp"
//
#include <mutex>

namespace ffgui {
/* ****************************************************************************************************************** */

class GuiNotify
{
   public:
    GuiNotify();
    void Loop(const double now);
    void Draw();

    static void Message(const std::string& _title, const std::string& _content = "", const double _duration = 5.0);
    static void Notice(const std::string& _title, const std::string& _content = "", const double _duration = 5.0);
    static void Warning(const std::string& _title, const std::string& _content = "", const double _duration = 5.0);
    static void Error(const std::string& _title, const std::string& _content = "", const double _duration = 5.0);
    static void Success(const std::string& _title, const std::string& _content = "", const double _duration = 5.0);

   protected:
    struct Notification
    {
        enum Type
        {
            MESSAGE,
            NOTICE,
            WARNING,
            ERROR,
            SUCCESS
        };
        Notification(
            const enum Type _type, const std::string& _title, const std::string& _content, const double _duration);
        enum Type type;
        std::string name;
        std::string title;
        std::string content;
        double created;
        double duration;
        double destroy;
        double remaining;
        bool hovered;
        static uint32_t notiNr;
    };

    static std::vector<Notification> notifications_;
    static std::mutex mutex_;
};

/* ****************************************************************************************************************** */
}  // namespace ffgui
#endif  // __GUI_WIDGET_NOTIFY_HPP__

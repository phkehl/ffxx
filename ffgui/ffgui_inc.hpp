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

#ifndef __FFGUI_INC_HPP__
#define __FFGUI_INC_HPP__

//
#include <algorithm>
#include <array>
#include <cinttypes>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>
//
#include <fpsdk_common/gnss.hpp>
#include <fpsdk_common/logging.hpp>
#include <fpsdk_common/string.hpp>
#include <fpsdk_common/time.hpp>
#include <fpsdk_common/types.hpp>
using namespace fpsdk::common::gnss;
using namespace fpsdk::common::logging;
using namespace fpsdk::common::string;
using namespace fpsdk::common::time;
using namespace fpsdk::common::types;
//
#include "IconsForkAwesome.h"
//
#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS
#include "imgui.h"
#include "imgui_stdlib.h"
//
#define IMPLOT_DISABLE_OBSOLETE_FUNCTIONS
#include "implot.h"
//
// #include "implot3d.h"
//
#include "utils.hpp"
#include "data.hpp"
//
#include "gui_global.hpp"
#include "gui_utils.hpp"
#include "gui_win.hpp"

//
#ifdef NDEBUG
#  define IF_DEBUG(_debug_, _release_) _release_
#else
#  define IF_DEBUG(_debug_, _release_) _debug_
#endif

#endif  // __FFGUI_INC_HPP__

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

#ifndef __INPUT_HPP__
#define __INPUT_HPP__

//
#include "ffgui_inc.hpp"
//
#include "receiver.hpp"
#include "logfile.hpp"
#include "database.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

enum InputType
{
    InputType_None = 0,
    InputType_Receiver = (1 << 0),
    InputType_Logfile = (1 << 1),
    InputType_Corr = (1 << 2),
};

inline InputType operator|(const InputType a, const InputType b)
{
    return static_cast<InputType>(static_cast<int>(a) | static_cast<int>(b));
}

struct Input
{
    Input() = delete;
    Input(const std::string& name, const DataSrcDst srcId, const InputType type) /* clang-format off */ :
        srcId_   { srcId },
        name_    { name },
        type_    { type }  // clang-format on
    {
    }
    const DataSrcDst srcId_;
    std::string name_;
    InputType type_;

    ReceiverPtr receiver_;
    LogfilePtr logfile_;
    ReceiverPtr corr_;

    DatabasePtr database_;
};

// clang-format off
inline bool operator<(const Input& lhs, const Input& rhs) { return lhs.name_ < rhs.name_; }
inline bool operator==(const Input& lhs, const Input& rhs) { return (lhs.srcId_ == rhs.srcId_); }
struct InputHasher { std::size_t operator()(const Input& k) const { return reinterpret_cast<std::uintptr_t>(k.srcId_); } };
// clang-format on

using InputPtr = std::shared_ptr<Input>;
using InputPtrMap = std::map<std::string, InputPtr>;


InputPtr CreateInput(const std::string& name, const DataSrcDst srcId, const InputType type);

void RemoveInput(const std::string& name);

void DrawInputDebug();

const InputPtrMap& GetInputs();

/* ****************************************************************************************************************** */
}  // namespace ffgui
#endif  // __INPUT_HPP__

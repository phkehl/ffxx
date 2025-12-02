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

#ifndef __UTILS_HPP__
#define __UTILS_HPP__

//
#include "ffgui_inc.hpp"
//
#include <string>
//
#include <nlohmann/json.hpp>
#include <fpsdk_common/ext/eigen_core.hpp>

namespace ffgui {
/* ****************************************************************************************************************** */

// 2d Vector
template <typename T>
struct Vec2
{
    // clang-format off
    T x = 0.0;
    T y = 0.0;
    Vec2() = default;
    Vec2(const T _x, const T _y) : x { _x }, y { _y } { }
    inline Vec2  operator *  (const T f)     const { return Vec2(x * f,   y * f);   }    // Vec2 * x
    inline Vec2  operator /  (const T f)     const { return Vec2(x / f,   y / f);   }    // Vec2 / x
    inline Vec2  operator *  (const Vec2 &v) const { return Vec2(x * v.x, y * v.y); }    // Vec2 * Vec2
    inline Vec2  operator /  (const Vec2 &v) const { return Vec2(x / v.x, y / v.y); }    // Vec2 / Vec2
    inline Vec2  operator +  (const Vec2 &v) const { return Vec2(x + v.x, y + v.y); }    // Vec2 + Vec2
    inline Vec2  operator -  (const Vec2 &v) const { return Vec2(x - v.x, y - v.y); }    // Vec2 - Vec2
    inline Vec2 &operator *= (const T f)           { x *= f;   y *= f; }                 // Vec2 *= x;
    inline Vec2 &operator /= (const T f)           { x /= f;   y /= f; }                 // Vec2 /= x;
    inline Vec2 &operator *= (const Vec2 &v)       { x *= v.x; y *= v.y; return *this; } // Vec2 *= Vec2;
    inline Vec2 &operator /= (const Vec2 &v)       { x /= v.x; y /= v.y; return *this; } // Vec2 /= Vec2;
    inline Vec2 &operator += (const Vec2 &v)       { x += v.x; y += v.y; return *this; } // Vec2 += Vec2;
    inline Vec2 &operator -= (const Vec2 &v)       { x -= v.x; y -= v.y; return *this; } // Vec2 -= Vec2;

    template<typename TT = float>
    Vec2(const Vec2<double> &v) : x{(T)v.x}, y{(T)v.y} { }                               // Vec2<float> = Vec2<double>

    template<typename TT = double>
    Vec2(const Vec2<float>  &v) : x{(T)v.x}, y{(T)v.y} { }                               // Vec2<double> = Vec2<float>

    // From/to imugi's ImVec2
    Vec2(const ImVec2 v) : x{v.x}, y{v.y} {}
    operator ImVec2() const { return ImVec2(x, y); }

    // From/to std::array
    Vec2(const std::array<T, 2> a) : x{a[0]}, y{a[1]} {}
    operator std::array<T, 2>() const { return { x, y }; }

    // clang-format on
};

// clang-format off
template <typename T>
inline Vec2<T> operator * (const T f, Vec2<T> const &v) { return v * f; }                // x * Vec2
// clang-format on

using Vec2f = Vec2<float>;
using Vec2d = Vec2<double>;

/* ****************************************************************************************************************** */

void WipeCache(const std::string& path, const double maxAge);

struct EnumeratedPort
{
    std::string port;
    std::string desc;
};
using EnumeratedPorts = std::vector<EnumeratedPort>;

EnumeratedPorts EnumeratePorts();

/* ****************************************************************************************************************** */
}  // namespace ffgui

namespace Eigen {

inline void to_json(nlohmann::json& j, const Vector3d& v)
{
    j = nlohmann::json::array({ v[0], v[1], v[2] });
}

inline void from_json(const nlohmann::json& j, Vector3d& v)
{
    j.at(0).get_to(v[0]);
    j.at(1).get_to(v[1]);
    j.at(2).get_to(v[2]);
}
}  // namespace Eigen

/* ****************************************************************************************************************** */
#endif  // __UTILS_HPP__

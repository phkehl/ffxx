/* ****************************************************************************************************************** */
// flipflip's gui (ffgui)
//
// Copyright (c) Philippe Kehl (flipflip at oinkzwurgl dot org),
// https://oinkzwurgl.org/hacking/glmatrix
//
// Based on xscreensaver glmatrix.c, Copyright (c) 1992-2018 Jamie Zawinski <jwz@jwz.org>
//
// Permission to use, copy, modify, distribute, and sell this software and its documentation for any purpose is hereby
// granted without fee, provided that the above copyright notice appear in all copies and that both that copyright
// notice and this permission notice appear in supporting documentation.  No representations are made about the
// suitability of this software for any purpose.  It is provided "as is" without express or implied warranty.
//
// GLMatrix -- simulate the text scrolls from the movie "The Matrix".
//
// This program does a 3D rendering of the dropping characters that appeared in the title sequences of the movies. See
// also `xmatrix' for a simulation of what the computer monitors actually *in* the movie did.

#ifndef __GLMATRIX_HPP__
#define __GLMATRIX_HPP__

#include <array>
#include <cstdint>
#include <glm/glm.hpp>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>

namespace GlMatrix {
/* ****************************************************************************************************************** */

class Renderer;
class Animator;

struct Options
{
    // clang-format off
    float       speed       = 1.0f;                  // Animation speed factor (how fast the glyphs move)
    float       density     = 20.0f;                 // Strip density [%]
    bool        doClock     = false;                 // Show time in some strips
    std::string timeFmt     = " %H:%M ";             // Time format (strftime() style)
    bool        doFog       = true;                  // Dim far away glyphs
    bool        doWaves     = true;                  // Brightness waves
    bool        doRotate    = true;                  // Slowly tilt and rotate view
    enum Mode_e { MATRIX, DNA, BINARY, HEXADECIMAL, DECIMAL };
    enum Mode_e mode = MATRIX;                       // Glyph selection
    bool        debugGrid   = false;                 // Render debug grid
    bool        debugFrames = false;                 // Render debug frames around glyphs
    std::array<uint8_t, 3> colour = {{ 0x00, 0xff, 0x00 }};  // Colour
    // clang-format on
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
    Options, speed, density, doClock, timeFmt, doFog, doWaves, doRotate, mode, debugGrid, debugFrames)

class Matrix
{
   public:
    Matrix();
    ~Matrix();

    bool Init(const Options& options = {});
    void Destroy();

    void Animate();
    void Render(const int width, const int height);

   private:
    std::unique_ptr<Renderer> renderer_;
    std::unique_ptr<Animator> animator_;
};

/* ****************************************************************************************************************** */
}  // namespace GlMatrix
#endif  // __GLMATRIX_HPP__

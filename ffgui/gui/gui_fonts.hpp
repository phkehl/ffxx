// clang-format off
// Automatically generated. Do not edit.
#ifndef __GUI_FONTS_HPP__
#define __GUI_FONTS_HPP__
#include <array>
namespace ffgui {
struct FontData
{
    const char* fontName;
    const char* dataRegular;
    const char* dataItalic;
    const char* dataBold;
};
static constexpr std::array<const char*, 3> FONT_NAMES = {{ "DejaVuMono", "B612Mono", "RobotoMono" }};
FontData GetFontData(const std::size_t ix);
const char* GetFontIcons();
}
#endif

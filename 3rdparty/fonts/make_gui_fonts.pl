#!/usr/bin/perl
use warnings;
use strict;
use FindBin;
use Path::Tiny;

my @FONTS =
(
    { name => 'DejaVuMono',     regular => 'DejaVuSansMono.ttf',         italic => 'DejaVuSansMono-Oblique.ttf', bold => 'DejaVuSansMono-Bold.ttf' },
    { name => 'B612Mono',       regular => 'B612Mono-Regular.ttf',       italic => 'B612Mono-Italic.ttf',        bold => 'B612Mono-Bold.ttf' },
    { name => 'RobotoMono',     regular => 'RobotoMono-Regular.ttf',     italic => 'RobotoMono-Italic.ttf',      bold => 'RobotoMono-Bold.ttf' },
    #{ name => '', regular => '', italic => '', bold => '' },

);
my $nFonts = $#FONTS + 1;

system("g++ -o $FindBin::Bin/binary_to_compressed_c $FindBin::Bin//binary_to_compressed_c.cpp");

my $hpp = '';
my $cpp = '';

$hpp .= "// clang-format off\n// Automatically generated. Do not edit.\n";
$hpp .= "#ifndef __GUI_FONTS_HPP__\n";
$hpp .= "#define __GUI_FONTS_HPP__\n";
$hpp .= "#include <array>\n";
$hpp .= "namespace ffgui {\n";
$hpp .= "struct FontData\n";
$hpp .= "{\n";
$hpp .= "    const char* fontName;\n";
$hpp .= "    const char* dataRegular;\n";
$hpp .= "    const char* dataItalic;\n";
$hpp .= "    const char* dataBold;\n";
$hpp .= "};\n";
$hpp .= "static constexpr std::array<const char*, $nFonts> FONT_NAMES = {{ " . join(", ", map { "\"$_->{name}\"" } @FONTS). " }};\n";
$hpp .= "FontData GetFontData(const std::size_t ix);\n";
$hpp .= "const char* GetFontIcons();\n";
$hpp .= "}\n";
$hpp .= "#endif\n";

$cpp .= "// clang-format off\n// Automatically generated. Do not edit.\n";
$cpp .= "#include \"gui_fonts.hpp\"\n";
$cpp .= "namespace ffgui {\n";

foreach my $font (@FONTS)
{
    $cpp .= qx{$FindBin::Bin/binary_to_compressed_c -base85 $FindBin::Bin/$font->{regular} $font->{name}Regular};
    $cpp .= qx{$FindBin::Bin/binary_to_compressed_c -base85 $FindBin::Bin/$font->{italic} $font->{name}Italic};
    $cpp .= qx{$FindBin::Bin/binary_to_compressed_c -base85 $FindBin::Bin/$font->{bold} $font->{name}Bold};
}

$cpp .= qx{$FindBin::Bin/binary_to_compressed_c -base85 $FindBin::Bin/ForkAwesome.ttf ForkAwesome};


$cpp .= "FontData GetFontData(const std::size_t ix) {\n";
$cpp .= "if (false) {}\n";
for (my $ix = 0; $ix <= $#FONTS; $ix++)
{
    my $font = $FONTS[$ix];
    $cpp .= "else if (ix == $ix) { return { \"$font->{name}\", $font->{name}Regular_compressed_data_base85, $font->{name}Italic_compressed_data_base85, $font->{name}Bold_compressed_data_base85 }; }\n";
}
$cpp .= "else { return GetFontData(0); }\n";
$cpp .= "}\n";
$cpp .= "const char* GetFontIcons() { return ForkAwesome_compressed_data_base85; }\n";
$cpp .= "}\n";

path("$FindBin::Bin/../../ffgui/gui/gui_fonts.hpp")->spew($hpp);
path("$FindBin::Bin/../../ffgui/gui/gui_fonts.cpp")->spew($cpp);

__END__
#!/bin/bash
set -e -x

script=$(readlink -f "$0")
base=$(dirname "$script")
echo "base=$base"
tmp="$base/fonts.tmp"
rm -rf "$tmp"
mkdir -p "$tmp"

g++ -o "$tmp/binary_to_compressed_c" "$base/binary_to_compressed_c.cpp"
c="$tmp/gui_fonts.cpp"
h="$tmp/gui_fonts.hpp"


function mkfont
{
    local name=$1
    local regular=$2
    local bold=$3
    local italic=$4
    $tmp/binary_to_compressed_c -base85 $regular ${name}Regular >> "$c"
    echo "const char *guiGetFont${name}Italic(void) { return ${name}Italic_compressed_data_base85; }" >> "$c"
    echo "const char *guiGetFont${name}Italic(void);" >> "$h"

    $tmp/binary_to_compressed_c -base85 $regular ${name}Regular >> "$c"
    echo "const char *guiGetFont${name}Bold(void) { return ${name}Bold_compressed_data_base85; }" >> "$c"
    echo "const char *guiGetFont${name}Bold(void);" >> "$h"
}

mkfont DejaVuSans DejaVuSansMono.ttf DejaVuSansMono-Oblique.ttf DejaVuSansMono-Bold.ttf

# B612Mono-BoldItalic.ttf
# B612Mono-Bold.ttf
# B612Mono-Italic.ttf
# B612Mono-Regular.ttf
# forkawesome-webfont.ttf


echo "#endif // __GUI_FONTS_HPP__" >> "$h"
cp "$h" "
cp "$c" "$base/../../ffgui/gui/gui_fonts.cpp"
rm -rf "$tmp"

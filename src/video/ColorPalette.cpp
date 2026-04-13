#include "video/ColorPalette.h"

std::array<PixelPair, 256> ColorPalette::buildPairLUT()
{
    std::array<PixelPair, 256> lut{};
    for (int i = 0; i < 256; i++) {
        lut[i].left  = colors[i & 0x0F];
        lut[i].right = colors[(i >> 4) & 0x0F];
    }
    return lut;
}

const std::array<PixelPair, 256> &ColorPalette::pairLUT()
{
    static const auto lut = buildPairLUT();
    return lut;
}

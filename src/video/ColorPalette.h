#pragma once

#include <array>
#include <cstdint>

struct PixelPair {
    uint32_t left;
    uint32_t right;
};

class ColorPalette {
public:
    // 16 C64 colors as RGBA (0xAABBGGRR little-endian for OpenGL)
    static constexpr std::array<uint32_t, 16> colors = {{
        0xFF000000,  // 0  Black
        0xFFF7F7F7,  // 1  White
        0xFF342F8D,  // 2  Red
        0xFFCDD46A,  // 3  Cyan
        0xFFA43598,  // 4  Purple
        0xFF42B44C,  // 5  Green
        0xFFB1292C,  // 6  Blue
        0xFF5DEFEF,  // 7  Yellow
        0xFF204E98,  // 8  Orange
        0xFF00385B,  // 9  Brown
        0xFF6D67D1,  // 10 Pink
        0xFF4A4A4A,  // 11 Dark Grey
        0xFF7B7B7B,  // 12 Medium Grey
        0xFF93EF9F,  // 13 Light Green
        0xFFEF6A6D,  // 14 Light Blue
        0xFFB2B2B2,  // 15 Light Grey
    }};

    static const std::array<PixelPair, 256> &pairLUT();

private:
    static std::array<PixelPair, 256> buildPairLUT();
};

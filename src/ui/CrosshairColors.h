#pragma once
#include <array>

// Physical X, Y and Z axes share the same colors in every view.
inline constexpr std::array<std::array<unsigned char, 3>, 3> CrosshairColors{{
    {{255, 82, 82}},   // X: red
    {{76, 217, 100}},  // Y: green
    {{66, 165, 245}}   // Z: blue
}};

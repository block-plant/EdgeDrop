#pragma once

#include <vector>
#include <cstdint>
#include <cstddef>

namespace lodepng {

/// Encodes an 8-bit indexed palette PNG with PLTE and optional tRNS chunks.
/// palette_rgba: flat array of RGBA bytes (size = num_colors * 4)
/// indices: width * height bytes (each in [0, num_colors - 1])
unsigned encode_indexed(std::vector<uint8_t>& out,
                        const uint8_t* indices,
                        unsigned width, unsigned height,
                        const std::vector<uint8_t>& palette_rgba);

/// Encodes a 32-bit RGBA PNG using Deflate level 9.
unsigned encode_rgba(std::vector<uint8_t>& out,
                     const uint8_t* rgba,
                     unsigned width, unsigned height);

} // namespace lodepng
